/*==================================================================================
    Copyright (c) 2008, binaryzebra
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    * Neither the name of the binaryzebra nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE binaryzebra AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL binaryzebra BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
=====================================================================================*/


#ifndef __QUICKED_PREVIEW_WIDGET_H__
#define __QUICKED_PREVIEW_WIDGET_H__

#include <QWidget>
#include <QPointer>
#include "ui_PreviewWidget.h"
#include "EditorSystems/EditorSystemsManager.h"
#include "EditorSystems/SelectionContainer.h"
#include <UI/UIControl.h>

namespace Ui
{
class PreviewWidget;
}
class EditorSystemsManager;

class Document;
class DavaGLWidget;
class ControlNode;
class ScrollAreaController;
class PackageBaseNode;
class RulerController;
class AbstractProperty;
class QWheelEvent;
class QNativeGestureEvent;

class PreviewWidget : public QWidget, public Ui::PreviewWidget
{
    Q_OBJECT
public:
    explicit PreviewWidget(QWidget* parent = nullptr);
    ~PreviewWidget();
    DavaGLWidget* GetGLWidget();
    ScrollAreaController* GetScrollAreaController();
    float GetScale() const;
    RulerController* GetRulerController();
    ControlNode* OnSelectControlByMenu(const DAVA::Vector<ControlNode*>& nodes, const DAVA::Vector2& pos);

signals:
    void DeleteRequested();
    void ImportRequested();
    void CutRequested();
    void CopyRequested();
    void PasteRequested();
    void SelectionChanged(const SelectedNodes& selected, const SelectedNodes& deselected);

public slots:
    void OnDocumentChanged(Document* document);
    void SaveSystemsContextAndClear();
    void LoadSystemsContext(Document* document);
    void OnSelectionChanged(const SelectedNodes& selected, const SelectedNodes& deselected);
    void OnRootControlPositionChanged(const DAVA::Vector2& pos);
    void OnNestedControlPositionChanged(const QPoint& pos);
    void OnEmulationModeChanged(bool emulationMode);

private slots:
    void OnScaleChanged(qreal scale);
    void OnScaleByComboIndex(int value);
    void OnScaleByComboText();

    void OnGLWidgetResized(int width, int height);

    void OnVScrollbarMoved(int position);
    void OnHScrollbarMoved(int position);

    void UpdateScrollArea();
    void OnPositionChanged(const QPoint& position);
    void OnGLInitialized();

protected:
    bool eventFilter(QObject* obj, QEvent* e) override;

private:
    void LoadContext();
    void SaveContext();

    void CreateActions();
    void ApplyPosChanges();
    void OnWheelEvent(QWheelEvent* event);
    void OnNativeGuestureEvent(QNativeGestureEvent* event);
    void OnPressEvent(QMouseEvent* event);
    void OnReleaseEvent(QMouseEvent* event);
    void OnMoveEvent(QMouseEvent* event);
    qreal GetScaleFromWheelEvent(int ticksCount) const;
    qreal GetNextScale(qreal currentScale, int ticksCount) const;
    qreal GetPreviousScale(qreal currentScale, int ticksCount) const;
    void OnSelectionInSystemsChanged(const SelectedNodes& selected, const SelectedNodes& deselected);
    void OnPropertiesChanged(const DAVA::Vector<ChangePropertyAction>& propertyActions, size_t hash);

    QPoint lastMousePos;
    QPointer<Document> document;
    DavaGLWidget* davaGLWidget = nullptr;
    ScrollAreaController* scrollAreaController = nullptr;
    QList<qreal> percentages;

    SelectionContainer selectionContainer;
    RulerController* rulerController = nullptr;
    QPoint rootControlPos;
    QPoint canvasPos;

    QAction* selectAllAction = nullptr;
    QAction* focusNextChildAction = nullptr;
    QAction* focusPreviousChildAction = nullptr;

    std::unique_ptr<EditorSystemsManager> systemsManager;
};

inline DavaGLWidget* PreviewWidget::GetGLWidget()
{
    return davaGLWidget;
}

#endif // __QUICKED_PREVIEW_WIDGET_H__
