#include "transactionnavigation.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <cstdio>
#include <cstdlib>

static void check(bool good, const char *message)
{
    if (!good) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
static void write(const QString &path, const QByteArray &data)
{
    QFile file(path);
    check(file.open(QIODevice::WriteOnly) && file.write(data) == data.size(), "write fixture");
}
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    check(temporary.isValid(), "temporary state directory");
    const auto path = temporary.filePath("client-navigation.json");
    const QString job(32, 'a'), first(32, 'b'), second(32, 'c');
    {
        TransactionNavigation state(path);
        check(!state.take("", job) && !state.take(first, "not-a-job"), "reject invalid targets");
        check(!QFile::exists(path), "ordinary launch does not create navigation state");
        check(state.take(first, job), "fresh notification opens once");
        check(!state.take(first, job), "repeated snapshot does not reopen Activity");
        check(state.take(second, job), "another click on the same transaction is fresh navigation");
        check(!state.take(first, job), "late older snapshot cannot reopen Activity");
    }
    {
        TransactionNavigation reopened(path);
        check(!reopened.take(first, job) && !reopened.take(second, job), "ordinary reopen suppresses old navigation");
        check(reopened.take(QString(32, 'd'), job), "fresh click after restart still opens its transaction");
    }
    const auto permissions = QFile::permissions(path);
    check(!(permissions & (QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ReadOther | QFileDevice::WriteOther)),
          "navigation state is private");
    const auto blocked = temporary.filePath("blocked");
    write(blocked, "not a directory");
    TransactionNavigation retry(blocked + "/state.json");
    check(!retry.take(first, job) && !retry.error().isEmpty(), "failed persistence does not consume navigation");
    check(QFile::remove(blocked), "remove blocked directory fixture");
    check(retry.take(first, job), "request can be retried when persistence recovers");
    const auto corrupt = temporary.filePath("corrupt.json");
    write(corrupt, "{");
    TransactionNavigation invalid(corrupt);
    check(!invalid.take(first, job) && !invalid.error().isEmpty(), "corrupt state does not replay unknown old targets");
    std::puts("PASS: fresh navigation, duplicate/late snapshots, client restart, private atomic state and failure recovery.");
}
