#include "vieworderwindow.h"
#include "ui_vieworderwindow.h"
#include <QPrinter>
#include <QPrintPreviewDialog>
#include <QPainter>
#include <QSqlQuery>
#include <QDate>

using namespace std;

ViewOrderWindow::ViewOrderWindow(QString orderNumber, ViewOrderInfo orderInfo, QWidget* parent) : QDialog(parent), ui(new Ui::ViewOrderWindow),
    _orderNumber(std::move(orderNumber)), _orderInfo(std::move(orderInfo))
{
    ui->setupUi(this);

    DisplayOrderInfo();
    SetupUI();

    connect(ui->payment, &QCheckBox::stateChanged, this, [=](int arg1) {
        if (arg1 == 2) {
            ui->printInvoice->setEnabled(true);
            ui->delivery->setEnabled(true);
        }
        else {
            ui->printInvoice->setEnabled(false);
            ui->delivery->setChecked(false);
            ui->delivery->setEnabled(false);
        }
    });

    connect(ui->delivery, &QCheckBox::stateChanged, this, [=](int arg1) {
        if (arg1 == 2) {
            ui->printWaybill->setEnabled(true);
        }
        else {
            ui->printWaybill->setEnabled(false);
        }
    });
}

void ViewOrderWindow::SetupUI() {
    this->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint);
    ui->verticalLayout->setAlignment(ui->orderNumberGroup, Qt::AlignCenter);
    ui->verticalLayout->setAlignment(ui->contractDateGroup, Qt::AlignCenter);
    ui->verticalLayout->setAlignment(ui->supplierGroup, Qt::AlignCenter);
    ui->verticalLayout->setAlignment(ui->paymentDeliveryGroup, Qt::AlignCenter);
    ui->verticalLayout->setAlignment(ui->ok, Qt::AlignCenter);
    SetupTables();
}

void ViewOrderWindow::SetupTables() {
    ui->goodsInOrder->setWordWrap(true);
    ui->goodsInOrder->resizeColumnsToContents();
    ui->goodsInOrder->horizontalHeader()->resizeSection(0, 250);
    QFont font = ui->goodsInOrder->horizontalHeader()->font();
    font.setBold(true);
    ui->goodsInOrder->horizontalHeader()->setFont(font);
    connect(ui->goodsInOrder->horizontalHeader(), &QHeaderView::sectionResized, ui->goodsInOrder, &QTableView::resizeRowsToContents);
    if (ui->goodsInOrder->horizontalHeader()->isHidden())
        ui->goodsInOrder->horizontalHeader()->show();

    ui->costInfo->horizontalHeader()->setDefaultSectionSize(135);
}

void ViewOrderWindow::DisplayOrderInfo() {
    // ?????????? ?????? ?????? ?? ?? ??????
    QSqlQuery orderPrice("SELECT price, price_nds "
                       "FROM receipts_sup "
                       "WHERE order_sup_num = '" + _orderNumber + "'");
    orderPrice.next();

    _goodsInOrderModel.setQuery("SELECT description, code, price, number, cost "
                                 "FROM ordered_sup_goods AS ord_goods "
                                 "JOIN catalog_sup AS cat ON ord_goods.good_code = cat.code "
                                 "WHERE order_sup_num = '" + _orderNumber + "'");
    _goodsInOrderModel.setHeaderData(0, Qt::Horizontal, "????????????????????????");
    _goodsInOrderModel.setHeaderData(1, Qt::Horizontal, "?????? ????????????");
    _goodsInOrderModel.setHeaderData(2, Qt::Horizontal, "????????");
    _goodsInOrderModel.setHeaderData(3, Qt::Horizontal, "??????-????");
    _goodsInOrderModel.setHeaderData(4, Qt::Horizontal, "??????????????????");

    ui->goodsInOrder->setModel(&_goodsInOrderModel);

    ui->orderNumber->setText(QString(_orderNumber).remove("??????-").simplified().remove(' '));
    ui->contractNumber->setText(QString(_orderInfo.contractNum).remove("??????-").simplified().remove(' '));
    ui->registrationDate->setText(QDate::fromString(_orderInfo.registrationDate, "yyyy-MM-dd").toString("dd.MM.yyyy"));
    ui->supplier->setText(_orderInfo.supplierName);

    ui->costInfo->item(0, 1)->setText(orderPrice.value(0).toString());
    ui->costInfo->item(0, 3)->setText(orderPrice.value(1).toString());

    if (_orderInfo.paid == "true") {
        ui->payment->setChecked(true);
        ui->payment->setEnabled(false);
        ui->printInvoice->setEnabled(true);
        if (_orderInfo.sent == "true") {
            ui->delivery->setChecked(true);
            ui->delivery->setEnabled(false);
            ui->printWaybill->setEnabled(true);
        }
        else {
            ui->delivery->setEnabled(true);
        }
    }
}

void ViewOrderWindow::on_ok_clicked() {
    QSqlQuery updateOrder;
    updateOrder.prepare("UPDATE orders_sup SET paid = :paid, sent = :sent WHERE num = :num");
    updateOrder.bindValue(":paid", ui->payment->isChecked());
    updateOrder.bindValue(":sent", ui->delivery->isChecked());
    updateOrder.bindValue(":num", _orderNumber);
    updateOrder.exec();

    this->parentWidget()->close();
    this->setResult(QDialog::Accepted);
}

void ViewOrderWindow::on_printInvoice_clicked() {
    QPrinter printer;
    QPrintPreviewDialog printWindow(&printer, nullptr, Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
    printWindow.setWindowTitle("???????????????? ???????????? ????????-??????????????");
    printWindow.resize(1000, 600);

    connect(&printWindow, &QPrintPreviewDialog::paintRequested, this, [=](QPrinter* printer) {

    QPainter painter;
    painter.begin(printer);

    int pageWidth = printer->pageLayout().paintRectPixels(printer->resolution()).width() - 20;
    int pageHeight = printer->pageLayout().paintRectPixels(printer->resolution()).height() - 20;
    QRect rect (20, 20, pageWidth, 0.1 * pageHeight);

    QPen pen = painter.pen();
    pen.setStyle(Qt::DotLine);
    painter.setPen(pen);

    QSqlQuery dateQuery ("SELECT date FROM receipts_sup WHERE order_sup_num = '??????-" + ui->orderNumber->text() + "'");
    dateQuery.next();

    QFont font = painter.font();
    font.setPixelSize(20);
    painter.setFont(font);
    painter.drawText(rect, Qt::AlignCenter, "????????-?????????????? ??? " + ui->orderNumber->text() +
    "         " + dateQuery.value(0).toString());

    QSqlQuery supQuery;
    supQuery.prepare("SELECT name, address, bank, inn, director, accountant "
    "FROM suppliers WHERE code = :code");

    supQuery.bindValue(":code", _orderInfo.supplierCode);
    supQuery.exec();
    supQuery.next();

    font.setPixelSize(15);
    painter.setFont(font);
    rect.setTop(0.12 * pageHeight);
    rect.setHeight(0.12 * pageHeight);
    rect.setLeft(20);
    rect.setWidth(pageWidth / 2 - 10);
    painter.drawText(rect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                        "???????????????? - \"" + supQuery.value(0).toString() + "\"\n"
                        "?????????????????????? ??????????:\n " + supQuery.value(1).toString() + "\n"
                        "?????????????????? ????????:\n " + supQuery.value(2).toString() + "\n"
                        "??????: " + supQuery.value(3).toString());

    QSqlQuery storeQuery ("SELECT name, address, bank, inn, accountant FROM stores");
    storeQuery.next();

    rect.setLeft(30 +pageWidth / 2);
    rect.setWidth(pageWidth / 2);
    //painter.drawRect(rect.adjusted(0, 0, -pen.width(), -pen.width()));
    painter.drawText(rect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                        "???????????????????? - \"" + storeQuery.value(0).toString() + "\"\n"
                        "?????????????????????? ??????????:\n " + storeQuery.value(1).toString() + "\n"
                        "?????????????????? ????????:\n " + storeQuery.value(2).toString() + "\n"
                        "??????: " + storeQuery.value(3).toString());
    font.setBold(true);
    painter.setFont(font);
    int top = 0.25 * pageHeight;

    rect.setTop(top);
    rect.setHeight(0.04 * pageHeight);
    rect.setLeft(10);
    rect.setWidth(200);//????????????????????????
    painter.drawText(rect, Qt::AlignCenter | Qt::TextWordWrap, "????????????????????????");
    rect.setLeft(210);
    rect.setWidth(100);//???????? ???? ??????????????
    painter.drawText(rect, Qt::AlignCenter | Qt::TextWordWrap, "???????? ???? ????., ??????");
    rect.setLeft(310);
    rect.setWidth(80);//??????-????
    painter.drawText(rect, Qt::AlignCenter | Qt::TextWordWrap, "??????-????");
    rect.setLeft(390);
    rect.setWidth(100);//?????????? ?????? ??????
    painter.drawText(rect, Qt::AlignCenter | Qt::TextWordWrap, "?????????? ?????? ??????");
    rect.setLeft(490);
    rect.setWidth(50);//?????? ????????????
    painter.drawText(rect, Qt::AlignCenter | Qt::TextWordWrap, "??????");
    rect.setLeft(540);
    rect.setWidth(100);//?????????? ????????????
    painter.drawText(rect, Qt::AlignCenter | Qt::TextWordWrap, "?????????? ????????????");
    rect.setLeft(640);
    rect.setWidth(100);//?????????????????? ?? ??????
    painter.drawText(rect, Qt::AlignCenter | Qt::TextWordWrap, "?????????????????? ?? ??????");
    font.setBold(false);
    painter.setFont(font);

    static QRegularExpression numExpression("(^(0|[1-9][0-9]{0,9})(,[0-9]{0,2})?)?");

    font.setPixelSize(14);
    painter.setFont(font);

    int rows = _goodsInOrderModel.rowCount();
    for(int i = 0; i < rows; i++) {
        top += 0.04 * pageHeight;

        if(top > pageHeight - 20 ){
            printer->newPage();
            top = 0.16 * pageHeight;
        }

        auto strPrice = _goodsInOrderModel.index(i, 2).data().toString().simplified().remove(' ');
        auto price = numExpression.match(strPrice).captured().replace(',','.').toDouble();
        double num = _goodsInOrderModel.index(i,3).data().toDouble();

        rect.setTop(top);
        rect.setHeight(0.04 * pageHeight);
        rect.setLeft(10);
        rect.setWidth(200);//????????????????????????
        painter.drawRect(rect.adjusted(0, 0, -pen.width(), -pen.width()));
        painter.drawText(rect, Qt::AlignCenter | Qt::TextWordWrap, _goodsInOrderModel.index(i,0).data().toString());

        rect.setLeft(210);
        rect.setWidth(100);//???????? ???? ??????????????
        painter.drawRect(rect.adjusted(0, 0, -pen.width(), -pen.width()));
        painter.drawText(rect, Qt::AlignCenter | Qt::TextWordWrap, QString::number(price));

        rect.setLeft(310);
        rect.setWidth(80);//??????-????
        painter.drawRect(rect.adjusted(0, 0, -pen.width(), -pen.width()));
        painter.drawText(rect, Qt::AlignCenter | Qt::TextWordWrap, QString::number(num));

        rect.setLeft(390);
        rect.setWidth(100);//?????????? ?????? ??????
        painter.drawRect(rect.adjusted(0, 0, -pen.width(), -pen.width()));
        painter.drawText(rect, Qt::AlignCenter, QString::number(price * num));

        rect.setLeft(490);
        rect.setWidth(50);//?????? ????????????
        painter.drawRect(rect.adjusted(0, 0, -pen.width(), -pen.width()));
        painter.drawText(rect, Qt::AlignCenter, "20 %");

        rect.setLeft(540);
        rect.setWidth(100);//?????????? ????????????
        painter.drawRect(rect.adjusted(0, 0, -pen.width(), -pen.width()));
        painter.drawText(rect, Qt::AlignCenter, QString::number(0.2 * price * num));

        rect.setLeft(640);
        rect.setWidth(100);//?????????????????? ?? ??????
        painter.drawRect(rect.adjusted(0, 0, -pen.width(), -pen.width()));
        painter.drawText(rect, Qt::AlignCenter, QString::number(1.2 * price * num));
    }
    rect.setTop(top + 0.04 * pageHeight);
    rect.setHeight(0.04 * pageHeight);
    rect.setLeft(10);
    rect.setWidth(380);
    painter.drawRect(rect.adjusted(0, 0, -pen.width(), -pen.width()));
    painter.drawText(rect, Qt::AlignCenter | Qt::TextWordWrap, "?????????? ?? ????????????, ?????????????? ??????");
    rect.setLeft(390);
    rect.setWidth(100);
    painter.drawRect(rect.adjusted(0, 0, -pen.width(), -pen.width()));
    painter.drawText(rect, Qt::AlignCenter | Qt::TextWordWrap, ui->costInfo->item(0, 1)->text());
    rect.setLeft(490);
    rect.setWidth(150);
    painter.drawRect(rect.adjusted(0, 0, -pen.width(), -pen.width()));
    rect.setLeft(640);
    rect.setWidth(100);
    painter.drawRect(rect.adjusted(0, 0, -pen.width(), -pen.width()));
    painter.drawText(rect, Qt::AlignCenter, ui->costInfo->item(0, 3)->text());

    rect.setTop(top + 0.1 * pageHeight);
    rect.setHeight(0.15 * pageHeight);
    rect.setLeft(20);
    rect.setWidth(pageWidth / 2-10);
    painter.drawText(rect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                        "????????????????:\n " + supQuery.value(4).toString() + "\n\n"
                        " ______________\n\n"
                        "?????????????? ??????????????????:\n " + supQuery.value(5).toString() + "\n\n"
                        " ______________");

    rect.setLeft(30 + pageWidth / 2);
    rect.setWidth(pageWidth / 2);
    //painter.drawRect(rect.adjusted(0, 0, -pen.width(), -pen.width()));
    painter.drawText(rect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                        "?????????????? ??????????????????:\n " + storeQuery.value(4).toString() + "\n\n"
                        " ______________");
    });

    printWindow.exec();
}

void ViewOrderWindow::on_printWaybill_clicked() {
    QPrinter printer;
    QPrintPreviewDialog printWindow(&printer, nullptr, Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
    printWindow.setWindowTitle("???????????????? ???????????? ???????????????? ??????????????????");
    printWindow.resize(1000, 600);

    connect(&printWindow, &QPrintPreviewDialog::paintRequested, this, [=](QPrinter* printer) {

    QPainter painter;
    painter.begin(printer);

    int pageWidth = printer->pageLayout().paintRectPixels(printer->resolution()).width() - 20;
    int pageHeight = printer->pageLayout().paintRectPixels(printer->resolution()).height() - 20;
    QRect rect (20, 20, pageWidth, 0.1 * pageHeight);

    QPen pen = painter.pen();
    pen.setStyle(Qt::DotLine);
    painter.setPen(pen);

    QSqlQuery dateQuery ("SELECT execution_date FROM orders_sup WHERE num = '??????-" + ui->orderNumber->text() + "'");
    dateQuery.next();

    QFont font = painter.font();
    font.setPixelSize(20);
    painter.setFont(font);
    painter.drawText(rect, Qt::AlignCenter, "???????????????? ?????????????????? ??? " + ui->orderNumber->text() +
    "      " + dateQuery.value(0).toString());


    QSqlQuery supQuery;
    supQuery.prepare("SELECT name, director FROM suppliers WHERE code = :code");
    supQuery.bindValue(":code", _orderInfo.supplierCode);
    supQuery.exec();
    supQuery.next();

    QSqlQuery storeQuery ("SELECT name, address FROM stores");
    storeQuery.next();

    font.setPixelSize(15);
    painter.setFont(font);
    rect.setTop(0.12 * pageHeight);
    rect.setHeight(0.12 * pageHeight);
    rect.setLeft(20);
    rect.setWidth(pageWidth);
    painter.drawText(rect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                        "???????????????????????????????? - \"" + supQuery.value(0).toString() + "\"\n"
                        "?????????????????? - \"" + supQuery.value(0).toString() + "\"\n\n"
                        "???????????????????? - \"" + storeQuery.value(0).toString() + "\"\n"
                        " ?????????? ????????????????: " + storeQuery.value(1).toString());

    font.setBold(true);
    painter.setFont(font);
    int top = 0.25 * pageHeight;

    rect.setTop(top);
    rect.setHeight(0.04 * pageHeight);
    rect.setLeft(10);
    rect.setWidth(200);
    painter.drawText(rect, Qt::AlignCenter | Qt::TextWordWrap, "????????????????????????");
    rect.setLeft(210);
    rect.setWidth(100);
    painter.drawText(rect, Qt::AlignCenter | Qt::TextWordWrap, "?????? ????????????");
    rect.setLeft(310);
    rect.setWidth(80);
    painter.drawText(rect, Qt::AlignCenter | Qt::TextWordWrap, "??????-????");
    rect.setLeft(390);
    rect.setWidth(100);
    painter.drawText(rect, Qt::AlignCenter | Qt::TextWordWrap, "???????? ???? ????., ??????");
    rect.setLeft(490);
    rect.setWidth(100);
    painter.drawText(rect, Qt::AlignCenter | Qt::TextWordWrap, "?????????? ?????? ??????");
    rect.setLeft(590);
    rect.setWidth(50);
    painter.drawText(rect, Qt::AlignCenter | Qt::TextWordWrap, "??????");
    rect.setLeft(640);
    rect.setWidth(100);
    painter.drawText(rect, Qt::AlignCenter | Qt::TextWordWrap, "?????????????????? ?? ??????");
    font.setBold(false);
    painter.setFont(font);

    static QRegularExpression numExpression("(^(0|[1-9][0-9]{0,9})(,[0-9]{0,2})?)?");

    font.setPixelSize(14);
    painter.setFont(font);

    int rows = _goodsInOrderModel.rowCount();
    for(int i = 0; i < rows; i++) {
        top += 0.04 * pageHeight;

        if(top > pageHeight - 20 ){
            printer->newPage();
            top = 0.16 * pageHeight;
        }

        auto strPrice = _goodsInOrderModel.index(i, 2).data().toString().simplified().remove(' ');
        auto price = numExpression.match(strPrice).captured().replace(',', '.').toDouble();
        double num = _goodsInOrderModel.index(i,3).data().toDouble();

        rect.setTop(top);
        rect.setHeight(0.04 * pageHeight);
        rect.setLeft(10);
        rect.setWidth(200);//????????????????????????
        painter.drawRect(rect.adjusted(0, 0, -pen.width(), -pen.width()));
        painter.drawText(rect, Qt::AlignCenter | Qt::TextWordWrap, _goodsInOrderModel.index(i,0).data().toString());

        rect.setLeft(210);
        rect.setWidth(100);//?????? ????????????
        painter.drawRect(rect.adjusted(0, 0, -pen.width(), -pen.width()));
        painter.drawText(rect, Qt::AlignCenter | Qt::TextWordWrap,
        _goodsInOrderModel.index(i,1).data().toString());

        rect.setLeft(310);
        rect.setWidth(80);//??????-????
        painter.drawRect(rect.adjusted(0, 0, -pen.width(), -pen.width()));
        painter.drawText(rect, Qt::AlignCenter | Qt::TextWordWrap, QString::number(num));

        rect.setLeft(390);
        rect.setWidth(100);//???????? ???? ??????????????
        painter.drawRect(rect.adjusted(0, 0, -pen.width(), -pen.width()));
        painter.drawText(rect, Qt::AlignCenter | Qt::TextWordWrap, QString::number(price));

        rect.setLeft(490);
        rect.setWidth(100);//?????????? ?????? ??????
        painter.drawRect(rect.adjusted(0, 0, -pen.width(), -pen.width()));
        painter.drawText(rect, Qt::AlignCenter, QString::number(price * num));

        rect.setLeft(590);
        rect.setWidth(50);//?????? ????????????
        painter.drawRect(rect.adjusted(0, 0, -pen.width(), -pen.width()));
        painter.drawText(rect, Qt::AlignCenter, "20 %");

        rect.setLeft(640);
        rect.setWidth(100);//?????????????????? ?? ??????
        painter.drawRect(rect.adjusted(0, 0, -pen.width(), -pen.width()));
        painter.drawText(rect, Qt::AlignCenter, QString::number(1.2 * price * num));
    }
    rect.setTop(top + 0.04 * pageHeight);
    rect.setHeight(0.04 * pageHeight);
    rect.setLeft(10);
    rect.setWidth(480);
    painter.drawRect(rect.adjusted(0, 0, -pen.width(), -pen.width()));
    painter.drawText(rect, Qt::AlignRight | Qt::AlignVCenter | Qt::TextWordWrap, "?????????? ");
    rect.setLeft(490);
    rect.setWidth(100);
    painter.drawRect(rect.adjusted(0, 0, -pen.width(), -pen.width()));
    painter.drawText(rect, Qt::AlignCenter | Qt::TextWordWrap, ui->costInfo->item(0, 1)->text());
    rect.setLeft(490);
    rect.setWidth(150);
    painter.drawRect(rect.adjusted(0, 0, -pen.width(), -pen.width()));
    rect.setLeft(640);
    rect.setWidth(100);
    painter.drawRect(rect.adjusted(0, 0, -pen.width(), -pen.width()));
    painter.drawText(rect, Qt::AlignCenter, ui->costInfo->item(0, 3)->text());

    rect.setTop(top + 0.1 * pageHeight);
    rect.setHeight(0.15 * pageHeight);
    rect.setLeft(20);
    rect.setWidth(pageWidth / 2-10);
    painter.drawText(rect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                        "???????????? ???????????????? ????????????????:\n " + supQuery.value(1).toString() + "\n\n"
                        " ______________");

    rect.setLeft(30 + pageWidth / 2);
    rect.setWidth(pageWidth / 2);
    painter.drawText(rect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                        "???????? ????????????:\n ?????????????????? ?????????????????? ??????????????????\n\n"
                        " ______________");
    });

    printWindow.exec();
}

ViewOrderWindow::~ViewOrderWindow() {
    done(result());
    delete ui;
}
