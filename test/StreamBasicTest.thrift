/*
 * Basic Stream Test - Simple struct with stream fields
 * Tests stream serialization/deserialization without RPC
 */

namespace cpp streamtest

struct StreamData {
  1: required string name,
  2: required stream<i32> numbers,
  3: required stream<string> messages
}

struct NestedStreamData {
  1: required i32 id,
  2: required stream<StreamData> data_stream
}
