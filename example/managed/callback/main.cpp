#include <point.hpp>
#include <rohit/managed.hpp>

#include <iostream>

// The runtime supplies its candidate point; do not retain this reference or change its ID.
void move_point(point& value) {
  value.x = 10;
  value.y = 20;
}

// Pass a named callback to the real transaction's low-level update operation.
int main() {
  rohit::managed::model_store<point> store{point{1, 2}};

  const auto outcome = store.execute_transaction("Move point", [](auto& transaction) {
    transaction.update(move_point);
  });
  outcome.throw_if_failed();

  const auto value = store.read();
  std::cout << "point: (" << value->x << ", " << value->y << ")\n";
  return value->x == 10 && value->y == 20 ? 0 : 1;
}
