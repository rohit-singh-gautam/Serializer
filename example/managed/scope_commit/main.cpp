#include <point.hpp>
#include <rohit/managed.hpp>

#include <iostream>

// Let the transaction destructor commit at healthy normal scope exit.
int main() {
  rohit::managed::model_store<point> store{point{1, 2}};
  rohit::managed::transaction_outcome outcome;

  {
    auto transaction = store.begin_transaction(outcome);
    auto editor = transaction.root();
    editor.set_x(10);
    editor.set_y(20);

    const auto before = store.read();
    std::cout << "inside scope: (" << before->x << ", " << before->y << ")\n";
  } // The transaction is destroyed here and attempts to commit.
  outcome.throw_if_failed();

  const auto after = store.read();
  std::cout << "after scope: (" << after->x << ", " << after->y << ")\n";
  return after->x == 10 && after->y == 20 ? 0 : 1;
}
