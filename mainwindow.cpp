/*
 * Author Telnov Victor, v-telnov@yandex.ru
 * This code under licence CC BY-NC-ND
 */

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <cmath>
#include <QHBoxLayout>
#include <QGraphicsView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScreen>
#include <QFileInfo>
#include <QFileDialog>
#include <QGraphicsPixmapItem>
#include <QWheelEvent>
#include <QScrollBar>

using std::vector;

#define DBL(x) static_cast<double>(x)

double gauss(double x) {
    return  1.0 / sqrt( 2.0 * M_PI ) * exp( - x*x / 2.0 );
}

struct Triple {
    double r = 0;
    double g = 0;
    double b = 0;

    QColor color() const {

        auto v = [](double vv) -> int {
            if (vv >= 255) return 255;
            return vv;
        };

        return QColor(v(r), v(g), v(b));
    }

    double get(int ii) const {
        if (ii == 0) return r;
        if (ii == 1) return g;
        if (ii == 2) return b;
        return 0; }

    Triple() {}

    Triple(double _r, double _g, double _b)
        : r(_r), g(_g), b(_b) {}

    Triple(const QColor& cl)
        : r(cl.red())
        , g(cl.green())
        , b(cl.blue()) {}
};

struct Avg {
    double su = 0;
    double weights = 0;

    void addValue(double v, double weight) {
        su += v * weight;
        weights += weight;
    }

    double mean() const {
        return su / weights; }
};

struct AvgColor {
    Avg rgb[3];

    void addTriple(const Triple& tr, double weight) {
        for (int ii = 0; ii < 3; ++ii)
            rgb[ii].addValue(tr.get(ii), weight);
    }

    Triple meanTriple() const {
        return Triple(rgb[0].mean(), rgb[1].mean(), rgb[2].mean());
    }
};

struct BluredImage {
    uint level = 0;
    vector<vector<Triple>> colors;
    QImage image;

    BluredImage* nextLevel = nullptr;
    ~BluredImage() { if (nextLevel) delete nextLevel; }
};

struct LoadedImage {
    QString path;
    QImage image;
    BluredImage* blured = nullptr;
    ~LoadedImage() { if (blured) delete blured; }
};

bool WheelEater::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::Wheel)
        return true;

    return QObject::eventFilter(obj, event);
}

void MainWindow::repaintImage(const QImage& image) {

    QPixmap p = QPixmap::fromImage(image);

    auto list = scene->items();
    if (list.size() == 0) {
        QGraphicsPixmapItem *pp = new QGraphicsPixmapItem(p);
        scene->addItem(pp);
    } else {
        auto *pp = static_cast<QGraphicsPixmapItem*>(list.first());
        pp->setPixmap(p);
    }
}

//******************************************
void MainWindow::repaintBlurImage(BluredImage *preLev, uint maLevel) {

    const QImage& srcImage = img->image;
    int w = srcImage.width();
    int h = srcImage.height();

    //**************************
    if (!preLev && !img->blured) {

        BluredImage *blured = new BluredImage;
        img->blured = blured;
        blured->colors.resize(w);

        for (int x = 0; x < w; ++x) {
            vector<Triple> &arTriple = img->blured->colors[x];
            arTriple.resize(h);
            for (int y = 0; y < h; ++y)
                arTriple[y] = srcImage.pixelColor(x, y);
        }
    }

    if (!preLev)
        return repaintBlurImage(img->blured, maLevel);

    //**************************

    auto makeRecu = [&] {
        BluredImage *blured = preLev->nextLevel;

        if (blured->level < maLevel)
            return repaintBlurImage(blured, maLevel);

        QImage& image = blured->image;

        if (!image.width()) {
            image = QImage(w, h, srcImage.format());
            for (int x = 0; x < w; ++x) {
                auto &arTr = blured->colors[x];
                for (int y = 0; y < h; ++y)
                    image.setPixelColor(x, y, arTr[y].color());
            }
        }

        repaintImage(image);
    };

    if (preLev->nextLevel)
        return makeRecu();

    const int sz = 5;
    const int sz2 = sz*sz;

    int dirs[sz2 * 2] = {};
    double weights[sz2] = {};

    int ii = 0;
    for (int ix = 0; ix < sz; ++ix)
        for (int iy = 0; iy < sz; ++iy) {
            int dx = ix - sz/2;
            int dy = iy - sz/2;

            dirs[ii*2] = dx;
            dirs[ii*2+1] = dy;

            double r = sqrt(dx*dx + dy*dy);
            weights[ii++] = gauss(r);
        }

    auto appendColor = [&](AvgColor& su, int x, int y, double weight) {
        if (x < 0 || x >= w) return;
        if (y < 0 || y >= h) return;
        su.addTriple(preLev->colors[x][y], weight);
    };

    auto getColor = [&](int x, int y) -> Triple {

        AvgColor res;

        for (int ii = 0; ii < sz2; ++ii)
            appendColor(
                res,
                x+dirs[ii*2],
                y+dirs[ii*2+1],
                weights[ii]);

        return res.meanTriple();
    };

    BluredImage *blured = new BluredImage;
    blured->level = preLev->level + 1;
    preLev->nextLevel = blured;

    blured->colors.resize(w);
    for (int x = 0; x < w; ++x) {
        vector<Triple> &arTr = blured->colors[x];
        arTr.resize(h);
        for (int y = 0; y < h; ++y) {
            arTr[y] = getColor(x, y);
        }
    }

    makeRecu();
}

void MainWindow::updateImage() {

    auto *ed = ui->ed_fileName;
    QString fileName = ed->text();

    if (img && fileName != img->path) {
        delete img;
        img = nullptr;
    }

    if (!img) {
        img = new LoadedImage;
        if (!fileName.isEmpty())
            img->image.load(fileName);

        img->path = fileName;
    }

    int blurLevel = ui->sld_blur->value();

    if (!blurLevel)
        return repaintImage(img->image);

    repaintBlurImage(nullptr, blurLevel);
}

void MainWindow::selectFile() {

    auto *ed = ui->ed_fileName;

    QFileInfo fi(ed->text());
    QString pa = fi.absolutePath();
    QString fileName = QFileDialog::getOpenFileName(
                this, "Open file", pa);

    if (!fileName.isNull()) {
        ed->setText(fileName);
        updateImage();
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    move(QGuiApplication::screens().at(0)->geometry().center() - frameGeometry().center());

    connect(ui->bt_selectFile, &QPushButton::released,
        [this] { selectFile(); });

    connect(ui->bt_exit, &QPushButton::released,
        [] { QApplication::quit(); });

    connect(ui->sld_blur, &QSlider::valueChanged,
        [this](int) { updateImage(); });

    auto *g = ui->graphicsView;
    g->viewport()->installEventFilter(new WheelEater);

    scene = new QGraphicsScene;
    g->setScene(scene);
}

MainWindow::~MainWindow()
{
    delete scene;
    delete ui;
    if (img) delete img;
}

void MainWindow::wheelEvent(QWheelEvent* event) {

    auto* w = ui->graphicsView;
    auto* v = w->viewport();

    QPointF g = event->globalPosition();
    // coordinate on the widget
    QPoint pt = w->mapFromGlobal(g.toPoint());
    // but only contains in the viewport
    if (!v->rect().contains(pt))
        return;

    QPointF sc_before = w->mapToScene(pt);

    double k = 1.15;

    if (event->angleDelta().y() < 0)
        k = 1.0 / k;

    w->scale(k, k);

    QPoint pt_after = w->mapFromScene(sc_before);

    auto *hs = w->horizontalScrollBar();
    auto *vs = w->verticalScrollBar();

    hs->setValue(hs->value() + pt_after.x() - pt.x());
    vs->setValue(vs->value() + pt_after.y() - pt.y());
}


