class CircularQueue:
    def __init__(self, size=4):
        self.size = size
        self.queue = [None] * size
        self.head = 0   # points to the front (next to dequeue)
        self.tail = 0   # points to the next free slot (next to enqueue)
        self.count = 0  # number of elements currently in the queue

    def is_full(self):
        return self.count == self.size

    def is_empty(self):
        return self.count == 0

    def enqueue(self, value):
        overwritten = None
        if self.is_full():
            # Overwrite the oldest element (the one at head).
            overwritten = self.queue[self.head]
            self.queue[self.tail] = value
            # tail and head both advance, so head moves to the next-oldest.
            self.tail = (self.tail + 1) % self.size
            self.head = (self.head + 1) % self.size
            # count stays at size
        else:
            self.queue[self.tail] = value
            self.tail = (self.tail + 1) % self.size
            self.count += 1
        return overwritten

    def dequeue(self):
        if self.is_empty():
            print("  Queue is empty! Cannot dequeue.")
            return None
        value = self.queue[self.head]
        self.queue[self.head] = None
        self.head = (self.head + 1) % self.size
        self.count -= 1
        return value

    def peek(self):
        """Return live items in FIFO order (oldest first)."""
        items = []
        for i in range(self.count):
            items.append(self.queue[(self.head + i) % self.size])
        return items

    def display(self):
        # Build the contents line. Each cell is given a fixed width
        # so the H/T markers below line up with the right position.
        cell_width = 6
        cells = []
        for i in range(self.size):
            val = self.queue[i] if self.queue[i] is not None else "."
            cells.append(f"[{str(val):^{cell_width-2}}]")
        contents_line = " ".join(cells)

        # Build the head/tail indicator line.
        marker_cells = []
        for i in range(self.size):
            marker = ""
            # When queue is empty we still show H and T (they overlap).
            # When queue is full, head and tail point to the same slot
            # but tail conceptually points to the slot AFTER the last
            # enqueued item, so we still draw both at that index.
            if i == self.head and i == self.tail:
                if self.is_empty():
                    marker = "H/T"
                else:  # full
                    marker = "H/T"
            elif i == self.head:
                marker = "H"
            elif i == self.tail:
                marker = "T"
            marker_cells.append(f"{marker:^{cell_width}}")
        marker_line = " ".join(marker_cells).rstrip()

        # Position labels for clarity
        position_cells = [f"{i:^{cell_width}}" for i in range(self.size)]
        position_line = " ".join(position_cells)

        print("  positions: " + position_line)
        print("  contents:  " + contents_line)
        print("  pointers:  " + marker_line)
        print(f"  (count = {self.count})")


def main():
    cq = CircularQueue(4)
    print("Circular Queue Demo (size = 4)")
    print("Enter a number to enqueue it (e.g. 27).")
    print("Enter one or more '-' to dequeue (e.g. --- dequeues 3 elements).")
    print("Enter 'p' to peek (show live items in FIFO order).")
    print("Enter 'q' to quit.\n")
    print("Initial state:")
    cq.display()
    print()

    while True:
        try:
            user_input = input("Command: ").strip()
        except EOFError:
            break

        if not user_input:
            continue
        if user_input.lower() in ("q", "quit", "exit"):
            print("Goodbye!")
            break

        # Peek: show live items in FIFO order without modifying the queue.
        if user_input.lower() == "p":
            items = cq.peek()
            if not items:
                print("  Peek: queue is empty.")
            else:
                print(f"  Peek (FIFO order, oldest first): {items}")
            print()
            continue

        # Dequeue: a string of one or more '-' characters.
        if all(ch == "-" for ch in user_input):
            k = len(user_input)
            for _ in range(k):
                removed = cq.dequeue()
                if removed is None:
                    break
                else:
                    print(f"  Dequeued {removed}.")
            cq.display()
            print()
            continue

        # Enqueue: a bare integer (no sign).
        try:
            value = int(user_input)
        except ValueError:
            print("  Invalid input. Enter a number to enqueue, or '-' chars to dequeue.\n")
            continue

        overwritten = cq.enqueue(value)
        if overwritten is not None:
            print(f"  Queue was full: overwrote {overwritten} with {value}.")
        else:
            print(f"  Enqueued {value}.")
        cq.display()
        print()


if __name__ == "__main__":
    main()
