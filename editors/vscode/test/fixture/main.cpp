#include <account.hpp>

// Verify that the generated owning type compiles with the attached runtime target.
int main() {
  account value{};
  return value.id == 0 ? 0 : 1;
}
