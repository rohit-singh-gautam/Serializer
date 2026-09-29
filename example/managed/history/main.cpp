#include <point.hpp>
#include <rohit/managed.hpp>

#include <iostream>

// Undo and redo the pair of coordinate changes as one revision.
int main() {
  rohit::managed::model_store<point> store{point{1, 2}};
  const auto outcome = store.execute_transaction("Move point", [](auto& transaction) {
    auto editor = transaction.root();
    editor.set_x(10);
    editor.set_y(20);
  });
  outcome.throw_if_failed();

  const auto moved = store.read();
  std::cout << "after edit: (" << moved->x << ", " << moved->y << ")\n";

  store.undo();
  const auto undone = store.read();
  std::cout << "after undo: (" << undone->x << ", " << undone->y << ")\n";

  store.redo(outcome.revision);
  const auto redone = store.read();
  std::cout << "after redo: (" << redone->x << ", " << redone->y << ")\n";
  return undone->x == 1 && undone->y == 2 && redone->x == 10 && redone->y == 20 ? 0 : 1;
}
