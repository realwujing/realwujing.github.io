#include <QObject>
#include <QDebug>

class Counter : public QObject
{
    Q_OBJECT
public:
    Counter();
    int value() const;
public slots:
    void setValue(int value);
signals:
    void valueChanged(int newValue);

private:
    int m_value;
};
