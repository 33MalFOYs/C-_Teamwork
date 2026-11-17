#include "memory_window.hpp"
#include "expense_tracker/memory_expense_service.hpp"
#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QApplication::setApplicationName("Expense Tracker - Temporary Workspace");
    expense_tracker::MemoryExpenseService expenses;
    expense_tracker::gui::MemoryWindow window(expenses);
    window.show();
    return application.exec();
}

