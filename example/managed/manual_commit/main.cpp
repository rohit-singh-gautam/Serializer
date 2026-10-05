#include <point.hpp>
#include <rohit/managed.hpp>

#include <iostream>

// Publish the edited candidate only when commit is explicitly called.
int main() {
  rohit::managed::model_store<point> store{point{1, 2}};
  rohit::managed::transaction_outcome outcome;

  {
    auto transaction = store.begin_transaction(outcome);
    auto editor = transaction.root();
    editor.set_x(10);
    editor.set_y(20);

    const auto before = store.read();
    std::cout << "before commit: (" << before->x << ", " << before->y << ")\n";
    transaction.commit();
  }
  outcome.throw_if_failed();

  const auto after = store.read();
  std::cout << "after commit: (" << after->x << ", " << after->y << ")\n";
  return after->x == 10 && after->y == 20 ? 0 : 1;
}
