/*
 * Author : Andy Creque (R-Zero take-home, April 2026)
 *
 * DESIGN NOTES
 * ============
 * 1. Due to my familiartity with the ESP-IDF and FreeRTOS APIs, this 
 *    implementation is somewhat specific to that environment, but the 
 *    architecture and design patterns are portable.
 * 2. The code provided is syntactially correct C code, but this program will 
 *    not compile and run due to missing headers, variable declarations, and 
 *    stubbed-out functions. 
 * 3. Queue decouples acquisition from transmission. The worker function 
 *    read_and_send_occupant_count() always drops data in the queue first, 
 *    followed by best-effort transmission of batches from the queue front. 
 *    Transmission is best-effort; nothing is lost on network failure.
 * 4. Queue is a circular buffer, static allocation — no heap fragmentation, 
 *    no malloc.
 * 5. Oldest-drop policy when queue is full. Assume recent occupancy data is 
 *    more actionable than stale readings.
 * 6. Commit-after-confirm. queue_find_batch() copies without advancing tail.
 *    queue_drain() advances tail only after network_send() succeeds. If send 
 *    fails, same data is retried next cycle.
 * 7. AES-CCM for application-layer encryption. Single-pass. Provides
 *    confidentiality + authenticity in one operation. Works reasonably well
 *    on constrained MCUs and is widely supported by crypto libraries.
 * 8. Key management placeholder. Production key must be loaded from encrypted
 *    non-volatile storage partition or external secure element. 
 */
#include "sensor_handler.h"

/* --------------------------------------------------------------------------
 * TODO: Include proper header files here.
 * -------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------
 * TODO: Add additional #defines here.
 * -------------------------------------------------------------------------- */
#define SENSOR_INTERVAL_MS   6000 // 10 rdgs per min = 6 sec between readings

/* --------------------------------------------------------------------------
 * Key data types
 * -------------------------------------------------------------------------- */
typedef struct {
    uint32_t timestamp;       // Unix epoch seconds — for cloud-side ordering 
    int16_t  occupant_count;  // Raw sensor value
    uint8_t  sequence_num;    // Wrapping 8-bit counter for deduplication
    uint8_t  _pad;            // Explicit alignment padding
} SensorReading_t;

typedef struct {
    SensorReading_t buf[QUEUE_CAPACITY];
    uint8_t         head;     // Next write index
    uint8_t         tail;     // Next read  index
    uint8_t         count;    // Current fill level
} ReadingQueue_t;

/* --------------------------------------------------------------------------
 * TODO - Add other global variable declarations and references here.
 * -------------------------------------------------------------------------- */
static ReadingQueue_t    s_queue;
static uint8_t           s_sequence   = 0;
static SemaphoreHandle_t s_mutex      = NULL;

/* --------------------------------------------------------------------------
 * Stubs — replace with real driver / network code
 * -------------------------------------------------------------------------- */
static int hal_read_sensor(void)
{
    return 3;
}
static uint32_t hal_get_timestamp(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000000ULL);
}
static bool hal_network_available(void)
{
    return true;
}
static int hal_network_send(const uint8_t *data, size_t len, uint32_t timeout_ms)
{
    return 0;
}

/* --------------------------------------------------------------------------
 * Circular queue operations - these functions are legitiment but untested.
 * -------------------------------------------------------------------------- */
static void queue_init(const ReadingQueue_t *q)
{
    memset(q, 0, sizeof(*q));
}
/* --------------------------------------------------------------------------
 * Drop one reading onto the queue. If full, drop oldest, advance head to make 
 * room.
 * -------------------------------------------------------------------------- */
static void queue_drop(ReadingQueue_t *q, const SensorReading_t *r)
{
    if (q->count >= QUEUE_CAPACITY) {
        q->tail  = (uint8_t)((q->tail + 1) % QUEUE_CAPACITY);
        q->count--;
    }
    q->buf[q->head] = *r;
    q->head  = (uint8_t)((q->head + 1) % QUEUE_CAPACITY);
    q->count++;
}
/* --------------------------------------------------------------------------
 * Find and copy up to *n readings from queue front into out[], without 
 * draining them from the queue. Updates *n to actual count copied. Returns 
 * true if at least one reading was found.
 * -------------------------------------------------------------------------- */
static bool queue_find_batch(const ReadingQueue_t *q, SensorReading_t *out, uint8_t *n)
{
    uint8_t batch = (*n < q->count) ? *n : q->count;
    for (uint8_t i = 0; i < batch; i++) {
        out[i] = q->buf[(q->tail + i) % QUEUE_CAPACITY];
    }
    *n = batch;
    return (batch > 0);
}
/* --------------------------------------------------------------------------
 * Advance tail by n. Call only after successful send. If n > count, queue is
 * fully drained.
 * -------------------------------------------------------------------------- */
static void queue_drain(ReadingQueue_t *q, uint8_t n)
{
    if (n > q->count) {
        n = q->count;
    }
    q->tail  = (uint8_t)((q->tail + n) % QUEUE_CAPACITY);
    q->count -= n;
}

/* --------------------------------------------------------------------------
 * Encryption - Encrypt a batch of SensorReading_t structs using AES-CCM.
 * 
 * readings    Plaintext batch
 * count       Number of readings in the batch
 * out_buf     Caller-allocated output buffer
 * out_buf_len Size of out_buf
 * out_len     Bytes written to out_buf on success
 * return      Success or failure indication
 * -------------------------------------------------------------------------- */
static esp_err_t encrypt_batch(const SensorReading_t *readings,
                               uint8_t count,
                               uint8_t *out_buf, size_t out_buf_len,
                               size_t  *out_len)
{
    /* ----------------------------------------------------------------------
     * TODO - Add calls to encryption library here. Use AES-256-GCM with a 
     *.       12-byte random nonce for efficiency.
     * ---------------------------------------------------------------------- */
    return ESP_OK;
}

/* --------------------------------------------------------------------------
 * Main API functions
 * -------------------------------------------------------------------------- */
int sensor_handler_init(void)
{
    /* Clear the queue, sequence number, & create mutext used to protect queue.
    ------------------------------------------------------------------------- */
    queue_init(&s_queue);
    s_sequence = 0;
    s_mutex = xSemaphoreCreateMutex();

    /* ----------------------------------------------------------------------
     * TODO - Load shared encryption key from non-volatile storage.
     * ---------------------------------------------------------------------- */
    return ESP_OK;
}
/* --------------------------------------------------------------------------
 * Here is where all the work is done. Called by timer callback every.
 * SENSOR_INTERVAL_MS.
 * -------------------------------------------------------------------------- */
void read_and_send_occupant_count(void)
{
     /* 1. Read sensor (outside mutex — sensor read can be slow) 
     ------------------------------------------------------------------------ */   
    SensorReading_t reading = {
        .timestamp      = hal_get_timestamp(),
        .occupant_count = (int16_t)hal_read_sensor(),
        .sequence_num   = s_sequence++,
    };

    /* 2. Drop reading in queue regardless of network state. This decouples 
          acquisition from transmission. Notice queue_drop() is outside the 
          mutex. This is safe because queue_drop() is the only function that 
          mutates the queue. Also, want to 
    ------------------------------------------------------------------------ */
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return; // Failed to acquire mutex.
    }
    queue_drop(&s_queue, &reading);
    xSemaphoreGive(s_mutex);

    /* 3. Check network availability. If it is down, nothing to do so exit.
    ------------------------------------------------------------------------ */
    if (!hal_network_available()) {
         return; // Network is down.
    }

    /* 4. Network is available — proceed with transmission.
    ------------------------------------------------------------------------ */
    while (1) {

        /* Find a batch (non-destructive).  
        -------------------------------------------------------------------- */
        SensorReading_t batch[TRANSMIT_BATCH_SIZE];
        uint8_t         n = TRANSMIT_BATCH_SIZE;

        if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            break;  // Failed to acquire mutex, exit and retry next time.
        }
        bool has_data = queue_find_batch(&s_queue, batch, &n);
        xSemaphoreGive(s_mutex);

        if (!has_data) {
            break;  // Queue empty, exit and retry next time.
        }

        /* Encrypt batch batch and stick it in payload.
        -------------------------------------------------------------------- */
        uint8_t payload[MAX_PAYLOAD_SIZE];
        size_t  payload_len = 0;
        if (encrypt_batch(batch, n, payload, sizeof(payload), &payload_len) != ESP_OK) {
            break;  // Encryption failed — this is rare, likely a bug.
        }

        /* Attempt to send batch over network. 
        -------------------------------------------------------------------- */
        int rc = hal_network_send(payload, payload_len, SEND_TIMEOUT_MS);
        if (rc != 0) {
            /* 
               TRANSMISSION FAILED! 

               Data remains in the queue, retry the same batch next time 
               through. If network is persistently down, queue_drop() will 
               overwrite oldest data .
            ---------------------------------------------------------------- */
            break;
        }

        /* Only try to drain batch from queue AFTER successful transmission.
        -------------------------------------------------------------------- */
        if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            queue_drain(&s_queue, n);
            xSemaphoreGive(s_mutex);
        }
    }
}

/* --------------------------------------------------------------------------
 * FreeRTOS timer callback — fires at SENSOR_INTERVAL_MS
 * -------------------------------------------------------------------------- */
static void sensor_timer_cb(TimerHandle_t xTimer)
{
    (void)xTimer;
    read_and_send_occupant_count();
}
/* --------------------------------------------------------------------------
 * MAIN ENTRY POINT
 * -------------------------------------------------------------------------- */
void app_main(void)
{
    /* Initialize sensor handler module - sets up queue and loads encryption key.
    --------------------------------------------------------------------------*/
    int ret = sensor_handler_init();
    if (ret != ESP_OK) {
        return; // Could not initialize sensor handler; nothing to do.
    }

    /* Create periodic timer — fires every SENSOR_INTERVAL_MS 
    --------------------------------------------------------------------------*/
    TimerHandle_t timer = xTimerCreate(
        "sensor_timer",
        pdMS_TO_TICKS(SENSOR_INTERVAL_MS),
        pdTRUE,              /* auto-reload */
        NULL,
        sensor_timer_cb
    );

    if (timer == NULL) {
        return; // Cound not create timer; nothing to do. 
    }

    if (xTimerStart(timer, 0) != pdPASS) {
        return; // Could not start timer; nothing to do.
    }

    /* Sit and spin waiting for timer events happening every SENSOR_INTERVAL_MS.
    --------------------------------------------------------------------------*/
    while (1); // Sit and spin waiting for timer events 
}
