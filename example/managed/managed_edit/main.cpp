#include <point.hpp>
#include <rohit/managed.hpp>

#include <iostream>

// Edit one generated point through a callback transaction.
int main() {
  rohit::managed::model_store<point> store{point{1, 2}};

  const auto outcome = store.execute_transaction([](auto& transaction) {
    auto editor = transaction.root();
    editor.set_x(10);
    editor.set_y(20);
  });
  outcome.throw_if_failed();

  const auto value = store.read();
  std::cout << "point: (" << value->x << ", " << value->y << ")\n";
  return value->x == 10 && value->y == 20 ? 0 : 1;
}
