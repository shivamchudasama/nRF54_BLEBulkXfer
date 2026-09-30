# BulkXfer — Protocol

This is the wire contract of BulkXfer **protocol version 2**: a reliable transfer of objects
of any size over BLE GATT. It says what two peers exchange and how each side must behave.
It says nothing about how an implementation is built, so a compatible peer can be written in
any language, on any BLE stack: Android, iOS, a PC with `bleak`, or another MCU.

| Document | Covers |
|---|---|
| **This file** | Wire format, GATT service, sequences, timing, error handling |
| [README.md](README.md) | Design rationale, MTU reasoning, integration of the C library |
| [API_REFERENCE.md](API_REFERENCE.md) | The C API of `_LIB/BulkXfer` |
| [../HexUpload/PROTOCOL.md](../HexUpload/PROTOCOL.md) | An application protocol built on top of this one |

There are two implementations: the C library in [_LIB/BulkXfer](../../_LIB/BulkXfer) (both
roles) and the Python client in
[_TOOLS/BleHostGUI/bulkxfer_client.py](../../_TOOLS/BleHostGUI/bulkxfer_client.py) (client
role). Both are tested against the byte vectors in
[_TEST/vectors/wire.json](../../_TEST/vectors/wire.json).

**MUST**, **MUST NOT**, **SHOULD** and **MAY** are used as in RFC 2119. Rules without these
words describe what the reference implementations do. A peer may differ there without
breaking interoperability.

---

## 1. Terms

| Term | Meaning |
|---|---|
| **Sender** | The side that sends an object. It is always the **GATT client**. |
| **Receiver** | The side that receives the object. It is always the **GATT server** and hosts the BulkXfer service. |
| **Object** | The bytes of one transfer, from 0 to 2³² − 1 bytes. |
| **Transfer** | One object moved from sender to receiver: START, then DATA frames, then END. |
| **Frame** | One protocol unit. Exactly one frame travels in each ATT write or notification. |
| **Short message** | An application frame that fits in a single frame. It has no handshake and no acknowledgement. |
| **Frame index** | The 0-based position of a DATA frame within the transfer, counted as a 32-bit number. Only its low 8 bits go on the wire, as `seq`. |
| **Window** | The largest number of DATA frames the sender may have sent but not yet had acknowledged. |

The sender and receiver roles are independent of the BLE link roles. Either the central or
the peripheral can be the sender. A device that sends in both directions hosts the service
**and** acts as a client of its peer's service, and runs the two directions independently.

---

## 2. GATT service

### 2.1 Attributes

| Attribute | Default UUID | Properties | Content |
|---|---|---|---|
| BulkXfer service (primary) | `B1C00000-16A1-4812-AF35-F3F29A92F6CA` | — | — |
| **DATA** | `B1C00001-16A1-4812-AF35-F3F29A92F6CA` | Write Without Response (**required**), Write (optional) | Sender → receiver frames |
| **CTRL** | `B1C00002-16A1-4812-AF35-F3F29A92F6CA` | Notify (**required**) | Receiver → sender frames |
| CTRL CCCD | `0x2902` | Read, Write | The sender writes `01 00` to enable notifications |
| **CAPS** (optional) | `B1C00003-16A1-4812-AF35-F3F29A92F6CA` | Read | 4-byte capability record (§2.3) |

A UUID is `<32-bit prefix>-<96-bit base>`. The prefix is
`| 8-bit domain 0xB1 | 8-bit service 0xC0 | 16-bit characteristic ID |`, where ID `0x0000`
is the service itself. The base `16A1-4812-AF35-F3F29A92F6CA` belongs to this project. A
deployment MAY use a different base or prefix, but both peers MUST use the same UUIDs.

- DATA MUST accept writes of every length from 2 bytes up to the receiver's maximum frame
  length (§3.4). Each write MUST carry exactly one whole frame. Long (prepared) writes and
  writes at a non-zero offset are not used and MUST be refused.
- Frames on CTRL are notifications. Indications are not used.
- The service MAY contain other attributes. A sender finds DATA and CTRL by UUID and
  properties, not by handle order.

### 2.2 Attaching

Before its first transfer on a connection, the sender MUST:

1. Discover the BulkXfer primary service.
2. Discover DATA (UUID plus the Write Without Response property) and CTRL (UUID plus the
   Notify property). If either is missing, the peer is not a BulkXfer receiver.
3. Find the CCCD of CTRL and write `0x0001` to it to enable notifications.

The receiver MUST NOT act on a START from a client that has not enabled CTRL notifications,
because no answer could reach that client. It ignores the START without replying (§7.1). Short
messages from such a client are still accepted.

The sender SHOULD also exchange the ATT MTU (to 247 if the stack allows) before its first
transfer, because the chunk size is fixed when the transfer starts (§5.1).

### 2.3 CAPS record

| Byte | Field | Value in this implementation |
|---|---|---|
| 0 | Protocol version | `2` |
| 1 | Largest frame the receiver accepts, in bytes (capped at 255) | `244` |
| 2 | Largest window the receiver grants | `16` |
| 3 | Reserved | `0` |

Example: `02 f4 10 00`. CAPS is informational. The sender does not have to read it, because
the window is negotiated in the START handshake (§5.3). Versioning is covered in §10.

### 2.4 Link settings (recommended)

The protocol works on any BLE link, down to the default ATT MTU of 23. Throughput depends on
the link, so both sides SHOULD request:

- ATT MTU 247, so that a frame fills one link-layer packet: 251 − 4 (L2CAP) = 247, and
  247 − 3 (ATT) = 244 bytes of frame.
- Data Length Extension with a 251-byte payload, and the 2M PHY.
- A short connection interval (7.5–15 ms). This is what matters most.

---

## 3. Frames

### 3.1 Common header

Every frame, in both directions, has this layout:

```
 offset  0        1        2 ...
        +--------+--------+---------------------------+
        |  len   |  type  |  payload (len bytes)      |
        +--------+--------+---------------------------+
```

- `len` (1 byte) is the payload length. `len + 2` MUST equal the length of the ATT value.
  Any other frame is malformed.
- `type` (1 byte) selects the frame:

| `type` | Kind |
|---|---|
| `0x00`–`0xEF` | **Application type**: a short message, whose payload belongs to the application |
| `0xF0`–`0xF5` | Framework frames (§3.2) |
| `0xF6`–`0xFF` | Reserved. Peers MUST NOT send these types and MUST drop any frame that has one |

Multi-byte fields are **little-endian**. Offsets in the tables below count from the first
byte of the frame, including `len` and `type`.

### 3.2 Framework frames

| `type` | Name | Sent by | Characteristic | `len` |
|---|---|---|---|---|
| `0xF0` | START | sender | DATA | 12 |
| `0xF1` | DATA | sender | DATA | 3 … 255 (2 + chunk length) |
| `0xF2` | ACK | receiver | CTRL | 3 |
| `0xF3` | NACK | receiver | CTRL | 3 |
| `0xF4` | END | receiver | CTRL | 2 |
| `0xF5` | ABORT | either | DATA if `dir` = 0, CTRL if `dir` = 1 | 3 |

Control frames have a fixed `len`. A frame whose `len` differs from the table is malformed.
A DATA frame MUST carry at least one data byte.

#### START — sender announces a transfer (14 bytes)

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 1 | `len` | `0x0C` |
| 1 | 1 | `type` | `0xF0` |
| 2 | 1 | `xferId` | Transfer identifier chosen by the sender (§5.4) |
| 3 | 1 | `appType` | Application type of the object, `0x00`–`0xEF` |
| 4 | 4 | `totalLen` | Object size in bytes, LE |
| 8 | 1 | `chunkSize` | Data bytes in every DATA frame except the last, `1`–`255` |
| 9 | 1 | `window` | Window the sender proposes, `1`–`128` |
| 10 | 4 | `crc32` | CRC-32 of the whole object (§4), LE |

Example (from a real upload): `0c f0 01 10 a4 19 00 00 f0 10 27 28 e7 e2` means xferId 1,
appType `0x10`, 6564 bytes, chunk 240, window 16, CRC `0xE2E72827`.

#### DATA — one chunk of the object (4 + N bytes)

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 1 | `len` | N + 2 |
| 1 | 1 | `type` | `0xF1` |
| 2 | 1 | `xferId` | Same as in START |
| 3 | 1 | `seq` | Frame index mod 256 (§5.2) |
| 4 | N | `data` | Object bytes `[index × chunkSize, index × chunkSize + N)` |

N MUST be `chunkSize` for every frame except the last, and `totalLen − (frames − 1) × chunkSize`
for the last one. Example: `05 f1 03 ff a0 a1 a2` means xferId 3, seq 255, 3 data bytes.

#### ACK — cumulative acknowledgement (5 bytes)

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 1 | `len` | `0x03` |
| 1 | 1 | `type` | `0xF2` |
| 2 | 1 | `xferId` | Transfer being acknowledged |
| 3 | 1 | `seq` | **Next expected** frame index mod 256. Every frame before it has been received |
| 4 | 1 | `window` | Window granted by the receiver, `1`–`128` |

ACK with `seq` = 0 in answer to START means the transfer is accepted. Example: `03 f2 01 00 10`.

#### NACK — go back (5 bytes)

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 1 | `len` | `0x03` |
| 1 | 1 | `type` | `0xF3` |
| 2 | 1 | `xferId` | Transfer concerned |
| 3 | 1 | `seq` | Next expected frame index mod 256. The sender resends from here |
| 4 | 1 | `reason` | `0x0A` NO_RESOURCES or `0x0B` OUT_OF_ORDER (§3.3) |

A NACK also acknowledges every frame before `seq`. Example: `03 f3 09 04 0a`.

#### END — final result (4 bytes)

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 1 | `len` | `0x02` |
| 1 | 1 | `type` | `0xF4` |
| 2 | 1 | `xferId` | Transfer that ended |
| 3 | 1 | `status` | `0x00` OK or `0x01` CRC_ERROR (§3.3) |

END replaces the ACK of the last frame. Only `status` = OK means the receiver has the whole
object and its CRC matched. Example: `02 f4 01 00`.

#### ABORT — cancel a transfer (5 bytes)

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 1 | `len` | `0x03` |
| 1 | 1 | `type` | `0xF5` |
| 2 | 1 | `xferId` | Transfer cancelled |
| 3 | 1 | `reason` | Status code (§3.3) |
| 4 | 1 | `dir` | `0x00` = sent by the sender (on DATA), `0x01` = sent by the receiver (on CTRL) |

`dir` identifies which transfer is meant. Two peers can each run a transfer to the other at
the same time with the same `xferId`. Example: `03 f5 09 05 01` means the receiver rejects
transfer 9.

### 3.3 Status codes

One code space is used for the END `status`, the NACK `reason` and the ABORT `reason`.

| Code | Name | On the wire in | Meaning |
|---|---|---|---|
| `0x00` | OK | END | Object complete, CRC matched |
| `0x01` | CRC_ERROR | END | All frames arrived, CRC did not match |
| `0x02` | TIMEOUT | ABORT | The peer stopped responding |
| `0x03` | ABORTED | ABORT | Cancelled by the application on the side that sends the ABORT |
| `0x04` | REMOTE_ABORTED | — | Local result only: the peer aborted |
| `0x05` | REJECTED | ABORT (`dir` 1) | The receiver refused the START |
| `0x06` | DISCONNECTED | — | Local result only: the link was lost |
| `0x07` | SOURCE_ERROR | ABORT (`dir` 0) | The sender could not read its object |
| `0x08` | SINK_ERROR | ABORT (`dir` 1) | The receiver could not store a chunk |
| `0x09` | PROTOCOL_ERROR | ABORT | Invalid START parameters or DATA of the wrong size |
| `0x0A` | NO_RESOURCES | NACK | The receiver dropped a frame because its queue was full |
| `0x0B` | OUT_OF_ORDER | NACK | The receiver saw a gap in the sequence |

A peer MUST accept any code in these fields, including ones it does not know, and treat an
unknown END status as a failure.

### 3.4 Frame size

- The largest frame on a link is `min(ATT_MTU − 3, receiver maximum)`. The receiver maximum
  is 244 in this implementation and is published in CAPS byte 1. A sender that does not read
  CAPS SHOULD assume 244.
- The largest short-message payload is that size minus 2 (242 at MTU 247).
- The largest `chunkSize` is that size minus 4 (240 at MTU 247, 182 at MTU 185, 16 at MTU 23).
- The `len` byte limits any frame to 257 bytes, so an ATT MTU above 260 gains nothing.

### 3.5 Receiving a frame

A peer MUST check every received frame in this order and drop it silently when a check fails:

1. The ATT value is at least 2 bytes long, and `len + 2` equals its length.
2. For a framework type, `len` matches §3.2. The type is not reserved.
3. The frame belongs on the characteristic it arrived on:
   - DATA carries short messages, START, DATA and ABORT with `dir` = 0.
   - CTRL carries short messages, ACK, NACK, END and ABORT with `dir` = 1.
4. For ACK, NACK, END, DATA and ABORT, `xferId` matches the transfer in progress in that
   direction. Anything else is stale and ignored.

A dropped frame is never answered. The timeouts in §8 recover from any frame that was needed.

---

## 4. CRC-32

The `crc32` field of START is the standard CRC-32 (CRC-32/ISO-HDLC, the one used by zlib,
Ethernet and PNG):

| Parameter | Value |
|---|---|
| Polynomial | `0x04C11DB7` (reflected: `0xEDB88320`) |
| Initial value | `0xFFFFFFFF` |
| Input / output reflected | yes / yes |
| Final XOR | `0xFFFFFFFF` |
| Check value, `"123456789"` | `0xCBF43926` |
| Empty object | `0x00000000` |

It covers the object bytes only, in offset order, with no frame headers. In Python this is
`zlib.crc32(obj)`. In Zephyr it is `crc32_ieee(obj, len)`, or `crc32_ieee_update()` starting
from 0.

---

## 5. Transfer mechanics

### 5.1 Chunking

The sender fixes `chunkSize` when it sends START, normally at the largest value the link
allows (§3.4). It does not change during the transfer, even if the MTU grows later.

```
frames       = ceil(totalLen / chunkSize)        (0 when totalLen = 0)
offset(i)    = i × chunkSize
length(i)    = min(chunkSize, totalLen − offset(i))
```

The receiver MUST reject a START whose `chunkSize` is 0 or larger than its own largest chunk,
with ABORT(PROTOCOL_ERROR).

### 5.2 Sequence numbers

DATA frame `i` (0-based) carries `seq = i mod 256`. Both sides keep 32-bit frame indices and
convert an incoming `seq` against a reference index `base`:

```
index = base + ((seq − base) mod 256)
```

For the sender, `base` is its lowest unacknowledged index. For the receiver, it is its next
expected index. The result is unambiguous because the window is never larger than 128.

### 5.3 Window negotiation

1. START carries the sender's proposal `Ws`, from 1 to 128. The receiver MUST reject 0 or a
   value above 128 with ABORT(PROTOCOL_ERROR).
2. The receiver grants `Wr = min(Ws, its own maximum)` and returns it in the ACK that accepts
   the START. It repeats the same `Wr` in every later ACK.
3. The sender uses `W = min(Ws, max(1, min(Wr, 128)))` for the rest of the transfer. The window
   field of later ACKs is ignored.

The sender MUST NOT have more than `W` DATA frames sent but unacknowledged.

### 5.4 Transfer identifier

`xferId` MUST differ from the `xferId` of the sender's previous transfer on the same link.
A wrapping counter is enough; the reference implementations start at 1. This matters for two
reasons:

- The receiver treats a START that repeats the active `xferId`, before any DATA has arrived,
  as a retransmission. It re-sends the ACK and does not start a new transfer (§7.1).
- Frames left over from an earlier transfer are recognised by their `xferId` and dropped.

### 5.5 One transfer per direction

Each direction carries one transfer at a time. The sender MUST NOT send a new START until
the previous transfer has ended for it: END or ABORT received, or given up after a timeout.
If a receiver gets a START with a new `xferId` while a transfer is active, it ends the old
transfer as REMOTE_ABORTED (without sending an ABORT) and handles the new START.

Short messages MAY be sent at any time, including during a transfer, in either direction.

---

## 6. Sender behaviour

### 6.1 States

| State | Meaning |
|---|---|
| IDLE | No transfer |
| WAIT_ACCEPT | START sent, waiting for ACK(seq 0), END or ABORT |
| SENDING | DATA flowing, waiting for END |

The sender keeps three counters: `next` (the next frame index to send), `acked` (every frame
below it is acknowledged), and `retries` (consecutive ACK timeouts).

| State | Event | Action | Next state |
|---|---|---|---|
| IDLE | Application sends an object | Compute the CRC, pick `xferId` and `chunkSize`, send START, start the ACK timer, `retries` = 0 | WAIT_ACCEPT |
| WAIT_ACCEPT | ACK, `seq` = 0 | Set `W` (§5.3), `next` = `acked` = 0, `retries` = 0, restart the ACK timer, start sending DATA | SENDING |
| WAIT_ACCEPT | ACK, `seq` ≠ 0 | Ignore | WAIT_ACCEPT |
| WAIT_ACCEPT, SENDING | END | Report `status` (OK means delivered) | IDLE |
| WAIT_ACCEPT, SENDING | ABORT, `dir` = 1 | Report REJECTED if `reason` = REJECTED, otherwise REMOTE_ABORTED | IDLE |
| SENDING | ACK, `a` = index(seq, `acked`), and `acked` < `a` ≤ `next` | `acked` = `a`, `retries` = 0, restart the ACK timer | SENDING |
| SENDING | ACK outside that range | Ignore (a duplicate or stale ACK MUST NOT restart the timer) | SENDING |
| SENDING | NACK, `a` = index(seq, `acked`), and `a` ≤ `next` | `acked` = `next` = `a` (go back), restart the ACK timer | SENDING |
| WAIT_ACCEPT | ACK timer expires | `retries` += 1. If `retries` > MAX_RETRIES: send ABORT(TIMEOUT, dir 0) and report TIMEOUT. Otherwise resend START and restart the timer | WAIT_ACCEPT or IDLE |
| SENDING | ACK timer expires | `retries` += 1. If `retries` > MAX_RETRIES: send ABORT(TIMEOUT, dir 0) and report TIMEOUT. Otherwise `next` = `acked` (Go-Back-N) and restart the timer | SENDING or IDLE |
| WAIT_ACCEPT, SENDING | Application cancels | Send ABORT(ABORTED, dir 0), report ABORTED | IDLE |
| SENDING | Object source fails | Send ABORT(SOURCE_ERROR, dir 0), report SOURCE_ERROR | IDLE |
| any | Link lost | Report DISCONNECTED. Nothing is sent | IDLE |

### 6.2 Sending DATA

In SENDING, while `next < frames` and `next − acked < W`, the sender writes DATA frame `next`
(Write Without Response) and increments `next`. It never writes past the window, and never
writes DATA before the START is accepted.

- The sender keeps no copy of the frames it has sent. A resend reads the object again at
  `index × chunkSize`, so the object must stay unchanged until the transfer ends.
- The last frame is never acknowledged by an ACK. END confirms it.
- A NACK can be followed by further NACKs for the same loss, for example when frames sent
  before the rewind arrive at the receiver after it. Each NACK is handled by the same rule.

---

## 7. Receiver behaviour

The receiver keeps, per transfer: `xferId`, `appType`, `chunkSize`, `W`, `totalLen`, the
announced CRC, the running CRC, `frames`, `expected` (next expected frame index), `sinceAck`
(in-order frames since the last ACK) and `nackSent` (the current gap has been reported).

### 7.1 On START

Checked in this order:

1. The client has not enabled CTRL notifications → ignore the START and send nothing.
2. A transfer is active with the same `xferId` and `expected` = 0 → the ACK was lost; send
   ACK(seq 0, `W`) again. Nothing else changes.
3. A transfer is active (different `xferId`, or data already received) → end it locally as
   REMOTE_ABORTED, then continue with the new START.
4. `chunkSize` = 0 or above the receiver's largest chunk, or `window` outside 1–128 → send
   ABORT(PROTOCOL_ERROR, dir 1).
5. The application refuses this `appType` or `totalLen`, or cannot receive at all → send
   ABORT(REJECTED, dir 1).
6. Otherwise accept: `W = min(window, own maximum)`, `expected` = 0.
   - `totalLen` = 0: check the CRC (it must be 0), send END immediately and finish.
   - Otherwise send ACK(seq 0, `W`) and start the idle timer.

An ABORT that rejects a START is the only reply to it. No END follows.

### 7.2 On DATA

For a DATA frame of the active transfer (other frames are dropped, §3.5), the receiver first
restarts the idle timer, then computes `d = (seq − expected) mod 256`:

| `d` | Meaning | Action |
|---|---|---|
| 128–255 | Behind: a duplicate from a resend | Drop it. Make sure an ACK goes out within ACK_DELAY, so that the sender moves on |
| 1–127 | Ahead: frames are missing | Drop it (Go-Back-N: nothing is buffered out of order). If `nackSent` is false, send NACK(`expected`, OUT_OF_ORDER) and set `nackSent` |
| 0 | In order | Continue below |

For an in-order frame:

1. The data length MUST equal `length(expected)` (§5.1). Otherwise send
   ABORT(PROTOCOL_ERROR, dir 1) and finish.
2. Deliver the data at offset `expected × chunkSize`. If the application cannot store it,
   send ABORT(SINK_ERROR, dir 1) and finish.
3. Update the running CRC. `expected` += 1, `sinceAck` += 1, `nackSent` = false.
4. If `expected` = `frames`: send END(OK) if the CRC matches, END(CRC_ERROR) otherwise, and
   finish. No ACK is sent for the last frame.
5. Otherwise, if `sinceAck` ≥ `max(W / 2, 1)`: send ACK(`expected`, `W`) now.
6. Otherwise, if no delayed ACK is pending, schedule one for ACK_DELAY from now. A pending
   delayed ACK is not pushed back, so a slow trickle of frames is still acknowledged in time.

Sending any ACK resets `sinceAck` to 0 and cancels the pending delayed ACK, because ACKs are
cumulative.

### 7.3 Other events

| Event | Action |
|---|---|
| A frame is dropped because the receive queue is full | Send NACK(`expected`, NO_RESOURCES) once the queue has been drained, and set `nackSent` |
| ABORT with `dir` = 0 and the active `xferId` | Finish as REMOTE_ABORTED. Nothing is sent |
| Idle timer expires (no DATA for IDLE_TIMEOUT) | Send ABORT(TIMEOUT, dir 1) and finish |
| Application cancels | Send ABORT(ABORTED, dir 1) and finish |
| Link lost | Finish as DISCONNECTED. Nothing is sent |

The receiver MUST treat delivered data as provisional until it sends END(OK). After any other
result the partial object is to be discarded.

---

## 8. Timing

Neither side needs to know the other's timer values. They only have to keep the
relationships below.

| Timer | Side | Reference value | Purpose |
|---|---|---|---|
| ACK_TIMEOUT | sender | 1000 ms | No ACK progress → resend START or go back |
| MAX_RETRIES | sender | 5 | Consecutive timeouts before giving up. The sender waits up to (MAX_RETRIES + 1) × ACK_TIMEOUT = 6 s |
| ACK_DELAY | receiver | 20 ms | Longest delay before acknowledging in-order frames |
| IDLE_TIMEOUT | receiver | 5000 ms | No DATA of the active transfer → abort |

- ACK_DELAY MUST be well below ACK_TIMEOUT. Otherwise a slow but healthy transfer triggers
  retransmissions.
- IDLE_TIMEOUT SHOULD be longer than ACK_TIMEOUT, so that a sender recovering from a loss
  resends before the receiver gives up.
- The sender restarts ACK_TIMEOUT only when the transfer makes progress: START accepted, a
  new cumulative ACK, or a NACK. Duplicate ACKs do not restart it.

---

## 9. Sequences

### 9.1 Normal transfer

A 6564-byte object at MTU 247 (chunk 240 → 28 frames, window 16), from
[_LOG/BulkXfer_GUI_Client.txt](../../_LOG/BulkXfer_GUI_Client.txt):

```
sender (client)                                          receiver (server)
  write CCCD 01 00                          ─────────►   CTRL notifications on
  START  0c f0 01 10 a4190000 f0 10 2728e7e2 ─DATA───►   accept, W = min(16, 16)
                                            ◄──CTRL───   ACK  03 f2 01 00 10     (next 0)
  DATA seq 0 … 15   (f2 f1 01 <seq> + 240 B) ─DATA───►
                                            ◄──CTRL───   ACK  03 f2 01 04 10     (next 4)
  DATA seq 16 … 19                           ─DATA───►
                                            ◄──CTRL───   ACK  03 f2 01 08 10     (next 8)
  …                                                      …
  DATA seq 27 (last, 84 B: 56 f1 01 1b …)    ─DATA───►   CRC matches
                                            ◄──CTRL───   END  02 f4 01 00        (OK)
```

The ACKs here come every 4 frames, not every `W/2` = 8. That is the 20 ms ACK_DELAY firing
between connection events: at the ~30 ms interval of this link, about 4 frames arrive per
event. Each ACK frees window space, and the sender tops the window back up at once.

### 9.2 Empty object

```
  START  0c f0 02 20 00000000 f0 10 00000000  ─DATA───►   totalLen 0, CRC 0 matches
                                              ◄──CTRL───   END  02 f4 02 00
```

### 9.3 Rejected

```
  START  0c f0 09 …                           ─DATA───►   application refuses
                                              ◄──CTRL───   ABORT 03 f5 09 05 01   (REJECTED, by receiver)
```

The sender reports REJECTED. [HexUpload](../HexUpload/PROTOCOL.md) §5 lists when its
receiver does this.

### 9.4 Frame lost at the receiver

```
  DATA seq 0 … 7                              ─DATA───►   0 … 3 delivered, queue full: 4 … 7 dropped
                                              ◄──CTRL───   NACK 03 f3 09 04 0a   (next 4, NO_RESOURCES)
  go back: next = acked = 4
  DATA seq 4 … 11                             ─DATA───►   delivered in order
```

BLE's link layer already acknowledges and orders packets, so a frame can only be lost inside
the receiver, as here, where its queue was full. The NACK recovers straight away. The ACK
timeout covers a lost NACK.

### 9.5 Silent receiver

```
  START                                       ─DATA───►   (not subscribed / START lost)
  … 1 s … START again  (×5, one per ACK_TIMEOUT)
  … 1 s … ABORT 03 f5 01 02 00                ─DATA───►   (TIMEOUT, by sender)
```

---

## 10. Versions and compatibility

- **Version 2** (this document) makes the sender the GATT client: START and DATA are writes to
  DATA, and ACK / NACK / END are notifications on CTRL.
- **Version 1** had the device send its data to the central as notifications. The frame
  layout was the same, but the characteristics carried different frames, so version 1 and
  version 2 peers cannot talk to each other.

The version is exchanged only through CAPS byte 0. A sender that reads CAPS SHOULD NOT start
a transfer if the version is not 2. No version field is carried in the frames, and none is
negotiated. A future incompatible change will increment the version.

Rules for extending version 2 without breaking it:

- Types `0xF6`–`0xFF` are dropped today (§3.5), so they are free for new framework frames.
- New status codes can be added; receivers already accept unknown codes (§3.3).
- CAPS byte 3 is reserved and MUST be sent as 0 and ignored on read.

---

## 11. Known limitations

- **END and ABORT are not retransmitted.** If an END is lost (the receiver's notification
  queue was full, or the sender's CTRL queue dropped it), the receiver has finished, but the
  sender resends its last window, gets no answer (DATA for a finished transfer is dropped),
  and reports TIMEOUT. The object did arrive. An application that needs certainty can
  confirm with a short message of its own, as HexUpload does with RESULT and STORED.
- **Short messages are best effort.** They have no ACK, no retransmission and no ordering
  guarantee relative to a transfer's control frames.
- **One transfer per direction per link.** Several objects are sent one after another.
- **No security of its own.** Integrity is the CRC-32, which detects corruption but not
  tampering. Confidentiality and authentication must come from BLE pairing or from the
  application.
- **Throughput depends on the link, not the window.** Above about 8 frames per connection
  event, a larger window gains nothing. The connection interval, PHY and data length decide
  the rate.
