#include "EphemeridesPlotWindow.h"

#include <qwt_scale_engine.h>

#include "CubePlotCurve.h"

namespace Isis {
  /**
   * Constructor, creates a new EphemeridesPlotWindow
   *
   * @param title
   * @param parent
   */
  EphemeridesPlotWindow::EphemeridesPlotWindow(QString title, QWidget *parent) :
                       PlotWindow(title, PlotCurve::EphemerisTime,
                                  PlotCurve::Kilometers, parent) {
    QwtText angleLabel("Angle", QwtText::PlainText);

    angleLabel.setColor(Qt::darkCyan);
    QFont font = angleLabel.font();
    font.setPointSize(13);
    font.setBold(true);
    angleLabel.setFont(font);
    plot()->enableAxis(QwtPlot::yRight);
    plot()->setAxisTitle(QwtPlot::yRight, angleLabel);

    setPlotBackground(Qt::white);
  }


  /**
   * Add a rotation curve to the plot.
   *
   *
   * @param curve
   */
  void EphemeridesPlotWindow::addRotation(CubePlotCurve *curve) {
    curve->attach(plot());
    plot()->replot();
  }
}
