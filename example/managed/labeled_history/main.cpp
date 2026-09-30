#include <point.hpp>
#include <rohit/managed.hpp>

#include <iostream>

// Opt into action labels for an undo/redo menu while keeping the point model unchanged.
int main() {
  namespace managed = rohit::managed;
  using point_store = managed::model_store<point, managed::history_mode::linear,
                                          managed::history_labels::enabled>;
  point_store store{point{1, 2}};
  const auto outcome = store.execute_transaction("Move point", [](auto& transaction) {
    auto editor = transaction.root();
    editor.set_x(10);
    editor.set_y(20);
  });
  outcome.throw_if_failed();

  std::cout << "Undo " << store.undo_label() << '\n';
  store.undo();
  std::cout << "Redo " << store.redo_label() << '\n';
  store.redo();
  const auto value = store.read();
  std::cout << "point: (" << value->x << ", " << value->y << ")\n";
  return store.undo_label() == "Move point" && value->x == 10 && value->y == 20 ? 0 : 1;
}
