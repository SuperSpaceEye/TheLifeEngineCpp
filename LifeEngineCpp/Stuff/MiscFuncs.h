//
// Created by spaceeye on 27.06.22.
//

#ifndef THELIFEENGINECPP_MISCFUNCS_H
#define THELIFEENGINECPP_MISCFUNCS_H

#include <string>
#include <sstream>
#include <iomanip>
#include <boost/lexical_cast.hpp>
#include <boost/lexical_cast/try_lexical_convert.hpp>

#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QLineEdit>


template<typename T>
struct result_struct {
    bool is_valid;
    T result;
};

template <typename T> std::string to_str(const T& t, int float_precision = 2) {
    std::stringstream stream;
    stream << std::fixed << std::setprecision(float_precision) << t;
    return stream.str();
}

class DescisionMessageBox : public QDialog {
    Q_OBJECT

private:
    QVBoxLayout *vertical_layout;
    QHBoxLayout *horizontal_layout;
    QPushButton *accept_button;
    QPushButton *decline_button;
    QLabel *content_label;
public:
    DescisionMessageBox(const QString& title, const QString& content,
                        const QString& accept_text, const QString& decline_text, QWidget* parent=0)
                        : QDialog(parent) {
    vertical_layout = new QVBoxLayout();
    horizontal_layout = new QHBoxLayout();
    accept_button = new QPushButton(accept_text, this);
    decline_button = new QPushButton(decline_text, this);
    content_label = new QLabel(content, this);

    setLayout(vertical_layout);
    vertical_layout->addWidget(content_label, 2);
    vertical_layout->addLayout(horizontal_layout, 1);
    horizontal_layout->addWidget(accept_button);
    horizontal_layout->addWidget(decline_button);

    connect(accept_button, &QPushButton::pressed, this, &QDialog::accept);
    connect(decline_button, &QPushButton::pressed, this, &QDialog::reject);

    this->setWindowTitle(title);
    }
};

int display_dialog_message(const std::string &message, bool disable_warnings);

void display_message(const std::string &message);

template<typename T>
result_struct<T> try_convert_message_box_template(const std::string& message, QLineEdit *line_edit, T &fallback_value) {
    T result;
    if (boost::conversion::detail::try_lexical_convert<T>(line_edit->text().toStdString(), result)) {
        return result_struct<T>{true, result};
    } else {
        display_message(message);
        line_edit->setText(QString::fromStdString(to_str(fallback_value, 5)));
        return result_struct<T>{false, result};
    }
}

#endif //THELIFEENGINECPP_MISCFUNCS_H