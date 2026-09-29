#include <point.hpp>
#include <rohit/managed.hpp>

#include <iostream>

// Create, read, and update a managed point generated from point.serializer.
int main() {
  rohit::managed::model_store<point> store{point{1, 2}};

  const auto before = store.read();
  std::cout << "before update: (" << before->x << ", " << before->y << ")\n";

  const auto outcome = store.execute_transaction("Move point", [](auto& transaction) {
    auto editor = transaction.root();
    editor.set_x(10);
    editor.set_y(20);
  });
  outcome.throw_if_failed();

  const auto after = store.read();
  std::cout << "after update: (" << after->x << ", " << after->y << ")\n";
  return before->x == 1 && before->y == 2 && after->x == 10 && after->y == 20 ? 0 : 1;
}
