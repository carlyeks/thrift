# Stream Wire Protocol Example

This document shows the exact wire protocol bytes for streaming vs non-streaming approaches using the `NumberService` example.

## Service Definition

```thrift
service NumberService {
  list<i64> getNumbers(1: i64 count)        // Traditional
  stream<i64> streamNumbers(1: i64 count)   // Streaming
}
```

## Example: Generate 3 numbers (42, 43, 44)

### Traditional List Approach: `getNumbers(3)`

**Wire format (Binary Protocol):**
```
// Message header
[4 bytes] version + message type  // 0x80010001 (version 1, CALL)
[4 bytes] method name length      // 0x0000000A ("getNumbers")
[10 bytes] "getNumbers"
[4 bytes] sequence id             // 0x00000001

// Arguments
[1 byte] struct begin             // field type (not sent in binary)
[1 byte] field type               // T_I64 (10)
[2 bytes] field id                // 0x0001 (field 1: count)
[8 bytes] count value             // 0x0000000000000003 (count=3)
[1 byte] field stop               // 0x00 (end of args struct)

// Response
[4 bytes] version + message type  // 0x80010002 (version 1, REPLY)
[4 bytes] method name length      // 0x0000000A ("getNumbers")
[10 bytes] "getNumbers"
[4 bytes] sequence id             // 0x00000001

// Return value (list<i64>)
[1 byte] field type               // T_LIST (15)
[2 bytes] field id                // 0x0000 (return value)
[1 byte] element type             // T_I64 (10)
[4 bytes] size                    // 0x00000003 (3 elements)
[8 bytes] element 1               // 0x000000000000002A (42)
[8 bytes] element 2               // 0x000000000000002B (43)
[8 bytes] element 3               // 0x000000000000002C (44)
[1 byte] field stop               // 0x00

TOTAL: ~69 bytes
Server must: Generate all 3 numbers, buffer in memory, then send all at once
```

### Stream Approach: `streamNumbers(3)`

**Wire format (Binary Protocol):**
```
// Message header (identical to above)
[4 bytes] version + message type  // 0x80010001 (version 1, CALL)
[4 bytes] method name length      // 0x0000000D ("streamNumbers")
[13 bytes] "streamNumbers"
[4 bytes] sequence id             // 0x00000001

// Arguments (identical to above)
[1 byte] struct begin
[1 byte] field type               // T_I64 (10)
[2 bytes] field id                // 0x0001
[8 bytes] count value             // 0x0000000000000003 (count=3)
[1 byte] field stop               // 0x00

// Response - STREAMING MODE
[4 bytes] version + message type  // 0x80010002 (version 1, REPLY)
[4 bytes] method name length      // 0x0000000D ("streamNumbers")
[13 bytes] "streamNumbers"
[4 bytes] sequence id             // 0x00000001

// Return value (T_STREAM type)
[1 byte] field type               // T_STREAM (17) ← dedicated stream type!
[2 bytes] field id                // 0x0000
[1 byte] element type             // T_I64 (10)

// Element 1 (can be sent immediately as generated)
[1 byte] has_more                 // 0x01 (T_STREAM_NEXT)
[8 bytes] element 1               // 0x000000000000002A (42)

// ... time passes, server generates next number ...

// Element 2
[1 byte] has_more                 // 0x01 (T_STREAM_NEXT)
[8 bytes] element 2               // 0x000000000000002B (43)

// ... time passes, server generates next number ...

// Element 3
[1 byte] has_more                 // 0x01 (T_STREAM_NEXT)
[8 bytes] element 3               // 0x000000000000002C (44)

// End of stream
[1 byte] has_more                 // 0x00 (T_STREAM_END)
[1 byte] field stop               // 0x00

TOTAL: ~71 bytes (4 bytes less overhead than size=-1 approach)
Server can: Generate each number on-demand, send immediately
Client can: Process each number as it arrives (no buffering!)
Old clients: Skip unknown T_STREAM type gracefully (no crash!)
```

## Key Differences

| Aspect | Traditional List | Stream (T_STREAM) |
|--------|-----------------|-------------------|
| Wire type | `T_LIST` (15) | `T_STREAM` (17) |
| Size field | `0x00000003` (3) | None - streaming! |
| Element framing | None - size known | `has_more` byte before each element |
| End marker | None - count to size | `has_more = 0x00` |
| Buffering | Must buffer all elements | Can send/process incrementally |
| Wire overhead (3 elem) | 5 bytes (type+id+etype+size) | 5 bytes (type+id+etype+3×has_more+end) |
| Wire overhead (1M elem) | 5 bytes | ~1MB (has_more bytes) |
| Memory (1M elements) | ~8MB minimum | ~Constant (streaming) |
| Old client behavior | Works normally | Gracefully skips (no crash!) |

## Error Handling Example

If the server encounters an error after sending 2 elements:

```
// Elements 1-2 sent successfully...
[1 byte] has_more                 // 0x01
[8 bytes] element 1               // 42
[1 byte] has_more                 // 0x01
[8 bytes] element 2               // 43

// Error occurs!
[1 byte] has_more                 // 0x02 (T_STREAM_ERROR)
[...] exception data              // Standard Thrift exception serialization
[1 byte] field stop               // 0x00

// Client receives: [42, 43] + exception
// Client can decide: keep partial results or discard all
```

## Backward Compatibility

**Old client reading streaming response:**
```cpp
// Old client code (doesn't know about T_STREAM type)
std::string name;
TType fieldType;
int16_t fieldId;
protocol->readFieldBegin(name, fieldType, fieldId);

if (fieldType == T_STREAM) {
  // Unknown type! Old protocol implementation will skip it
  protocol->skip(T_STREAM);
  // Stream field is treated as missing (like unset optional field)
  // No crash, no OOM allocation - just graceful degradation
} else if (fieldType == T_LIST) {
  // Normal list - old client can read this
  TType elemType;
  uint32_t size;
  protocol->readListBegin(elemType, size);

  vector<int64_t> numbers;
  for (uint32_t i = 0; i < size; i++) {
    int64_t num;
    protocol->readI64(num);
    numbers.push_back(num);
  }
  protocol->readListEnd();
}
protocol->readFieldEnd();
```

The old client **gracefully degrades** - it skips the unknown T_STREAM field using the protocol's skip() function. No crash, no 4GB allocation attempt!

## Stream-Aware Client

```cpp
// New stream-aware client code
std::string name;
TType fieldType;
int16_t fieldId;
protocol->readFieldBegin(name, fieldType, fieldId);

if (fieldType == T_STREAM) {
  // Stream type - process incrementally!
  TType elemType;
  protocol->readStreamBegin(elemType);

  while (true) {
    uint8_t has_more;
    protocol->readByte(has_more);

    if (has_more == T_STREAM_END) break;  // 0

    if (has_more == T_STREAM_ERROR) {  // 2
      // Read and throw exception
      throw readException();
    }

    // has_more == T_STREAM_NEXT (1)
    int64_t num;
    protocol->readI64(num);
    processNumber(num);  // ← Process immediately, no buffering!
  }

  protocol->readStreamEnd();
} else if (fieldType == T_LIST) {
  // Normal list - read all at once
  TType elemType;
  uint32_t size;
  protocol->readListBegin(elemType, size);

  for (uint32_t i = 0; i < size; i++) {
    int64_t num;
    protocol->readI64(num);
    processNumber(num);
  }
  protocol->readListEnd();
}
protocol->readFieldEnd();
```

## Performance Comparison

**Scenario: Stream 1,000,000 numbers**

| Metric | Traditional List | Stream (T_STREAM) |
|--------|-----------------|-------------------|
| Wire bytes | ~8,000,069 | ~9,000,069 (+12%) |
| Server memory | ~8 MB | ~constant |
| Client memory (old) | ~8 MB | ~0 (skips field!) |
| Client memory (new) | ~8 MB | ~constant |
| Time to first element | ~seconds | ~milliseconds |
| Cancelation support | No | Yes (stop reading) |
| Old client compatibility | Works normally | Skips field (graceful degradation) |

The ~1 MB overhead (12%) for streaming markers is small compared to the memory savings and latency improvements!

**Note**: Old clients skip T_STREAM fields entirely, treating them as missing optional fields. This is safer than attempting to read them, which would require protocol upgrades.
