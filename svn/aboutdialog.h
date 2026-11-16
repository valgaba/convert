#pragma once
#include <QDialog>

class AboutDialog : public QDialog {
public:
    explicit AboutDialog(QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
};
