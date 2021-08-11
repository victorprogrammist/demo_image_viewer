/*
 * Author Telnov Victor, v-telnov@yandex.ru
 * This code under GNU GPL licence.
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QLineEdit;
class QGraphicsScene;
struct BluredImage;
struct LoadedImage;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void repaintImage(const QImage& image);
    void repaintBlurImage(BluredImage *preLev, uint maLev);
    void updateImage();
    void selectFile();
    bool eventFilter(QObject *obj, QEvent *event) override;
    bool processWheelEvent(QWheelEvent* event);

    QGraphicsScene *scene = nullptr;
    LoadedImage *img = nullptr;

private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
