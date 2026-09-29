#include <point.hpp>
#include <rohit/managed.hpp>

#include <iostream>

// Read and change candidate coordinates using only the generated point editor.
int main() {
  rohit::managed::model_store<point> store{point{1, 2}};

  const auto outcome = store.execute_transaction("Move by one", [](auto& transaction) {
    auto editor = transaction.root();
    editor.set_x(editor.get_x() + 1);
    editor.set_y(editor.get_y() + 1);
  });
  outcome.throw_if_failed();

  const auto value = store.read();
  std::cout << "point: (" << value->x << ", " << value->y << ")\n";
  return value->x == 2 && value->y == 3 ? 0 : 1;
}
