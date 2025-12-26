/*
 * Simple Thrift file for testing template-based code generation
 */

namespace * template.test

// Simple enum
enum Status {
  OK = 0,
  WARNING = 1,
  ERROR = 2
}

// Simple struct
struct User {
  1: required i32 id,
  2: required string name,
  3: optional string email,
  4: Status status = Status.OK
}

// Service with a few methods
service UserService {
  User getUser(1: i32 id),
  list<User> getAllUsers(),
  void deleteUser(1: i32 id)
}
