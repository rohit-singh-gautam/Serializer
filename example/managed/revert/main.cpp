#include <point.hpp>
#include <rohit/managed.hpp>

#include <iostream>

// Cancel the entire unpublished edit, including setters which already succeeded.
int main() {
  rohit::managed::model_store<point> store{point{1, 2}};
  const auto outcome = store.execute_transaction([](auto& transaction) {
    auto editor = transaction.root();
    editor.set_x(10);
    editor.set_y(20);
    transaction.revert();
  });
  outcome.throw_if_failed();

  const auto value = store.read();
  std::cout << "after revert: (" << value->x << ", " << value->y << ")\n";
  return outcome.status == rohit::managed::transaction_status::reverted &&
                 value->x == 1 && value->y == 2
             ? 0
             : 1;
}
