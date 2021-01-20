// SPDX-License-Identifier: GPL-2.0
#include "boxseries.h"
#include "informationbox.h"
#include "statsaxis.h"
#include "statscolors.h"
#include "statshelper.h"
#include "statstranslations.h"
#include "statsview.h"
#include "zvalues.h"

#include <QLocale>

// Constants that control the bar layout
static const double boxWidth = 0.8; // 1.0 = full width of category
static const int boxBorderWidth = 2.0;

BoxSeries::BoxSeries(StatsView &view, StatsAxis *xAxis, StatsAxis *yAxis,
		     const QString &variable, const QString &unit, int decimals) :
	StatsSeries(view, xAxis, yAxis),
	variable(variable), unit(unit), decimals(decimals), highlighted(-1)
{
}

BoxSeries::~BoxSeries()
{
}

BoxSeries::Item::Item(StatsView &view, BoxSeries *series, double lowerBound, double upperBound,
		      const StatsQuartiles &q, const QString &binName) :
	lowerBound(lowerBound), upperBound(upperBound), q(q),
	binName(binName)
{
	item = view.createChartItem<ChartBoxItem>(ChartZValue::Series, boxBorderWidth);
	highlight(false);
	updatePosition(series);
}

BoxSeries::Item::~Item()
{
}

void BoxSeries::Item::highlight(bool highlight)
{
	if (highlight)
		item->setColor(highlightedColor, highlightedBorderColor);
	else
		item->setColor(fillColor, ::borderColor);
}

void BoxSeries::Item::updatePosition(BoxSeries *series)
{
	StatsAxis *xAxis = series->xAxis;
	StatsAxis *yAxis = series->yAxis;
	if (!xAxis || !yAxis)
		return;

	double delta = (upperBound - lowerBound) * boxWidth;
	double from = (lowerBound + upperBound - delta) / 2.0;
	double to = (lowerBound + upperBound + delta) / 2.0;

	double fromScreen = xAxis->toScreen(from);
	double toScreen = xAxis->toScreen(to);
	double q1 = yAxis->toScreen(q.q1);
	double q3 = yAxis->toScreen(q.q3);
	QRectF rect(fromScreen, q3, toScreen - fromScreen, q1 - q3);
	item->setBox(rect, yAxis->toScreen(q.min), yAxis->toScreen(q.max), yAxis->toScreen(q.q2));
}

void BoxSeries::append(double lowerBound, double upperBound, const StatsQuartiles &q, const QString &binName)
{
	items.emplace_back(new Item(view, this, lowerBound, upperBound, q, binName));
}

void BoxSeries::updatePositions()
{
	for (auto &item: items)
		item->updatePosition(this);
}

// Attention: this supposes that items are sorted by position and no box is inside another box!
int BoxSeries::getItemUnderMouse(const QPointF &point)
{
	// Search the first item whose "end" position is greater than the cursor position.
	auto it = std::lower_bound(items.begin(), items.end(), point.x(),
			   [] (const std::unique_ptr<Item> &item, double x) { return item->item->getRect().right() < x; });
	return it != items.end() && (*it)->item->getRect().contains(point) ? it - items.begin() : -1;
}

static QString infoItem(const QString &name, const QString &unit, int decimals, double value)
{
	QLocale loc;
	QString formattedValue = loc.toString(value, 'f', decimals);
	return unit.isEmpty() ? QStringLiteral(" %1: %2").arg(name, formattedValue)
			      : QStringLiteral(" %1: %2 %3").arg(name, formattedValue, unit);
}

std::vector<QString> BoxSeries::formatInformation(const Item &item) const
{
	QLocale loc;
	return {
		StatsTranslations::tr("%1 (%2 dives)").arg(item.binName, loc.toString((int)item.q.dives.size())),
		QStringLiteral("%1:").arg(variable),
		infoItem(StatsTranslations::tr("min"), unit, decimals, item.q.min),
		infoItem(StatsTranslations::tr("Q1"), unit, decimals, item.q.q1),
		infoItem(StatsTranslations::tr("median"), unit, decimals, item.q.q2),
		infoItem(StatsTranslations::tr("Q3"), unit, decimals, item.q.q3),
		infoItem(StatsTranslations::tr("max"), unit, decimals, item.q.max)
	};
}

// Highlight item when hovering over item
bool BoxSeries::hover(QPointF pos)
{
	int index = getItemUnderMouse(pos);
	if (index == highlighted) {
		if (information)
			information->setPos(pos);
		return index >= 0;
	}

	unhighlight();
	highlighted = index;

	// Highlight new item (if any)
	if (highlighted >= 0 && highlighted < (int)items.size()) {
		Item &item = *items[highlighted];
		item.highlight(true);
		if (!information)
			information = view.createChartItem<InformationBox>();
		information->setText(formatInformation(item), pos);
		information->setVisible(true);
	} else {
		information->setVisible(false);
	}
	return highlighted >= 0;
}

void BoxSeries::unhighlight()
{
	if (highlighted >= 0 && highlighted < (int)items.size())
		items[highlighted]->highlight(false);
	highlighted = -1;
}
