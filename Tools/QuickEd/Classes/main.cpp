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


#include <QApplication>
#include "UI/mainwindow.h"

#include "EditorCore.h"

#include "Platform/Qt5/QtLayer.h"
#include "QtTools/DavaGLWidget/davaglwidget.h"
#include "QtTools/FrameworkBinding/DavaLoop.h"
#include "QtTools/FrameworkBinding/FrameworkLoop.h"
#include "TextureCompression/PVRConverter.h"

void InitPVRTexTool()
{
#if defined (__DAVAENGINE_MACOS__)
    const DAVA::String pvrTexToolPath = "~res:/PVRTexToolCLI";
#elif defined (__DAVAENGINE_WIN32__)
    const DAVA::String pvrTexToolPath = "~res:/PVRTexToolCLI.exe";
#endif
    DAVA::PVRConverter::Instance()->SetPVRTexTool(pvrTexToolPath);
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setOrganizationName("DAVA");
    a.setApplicationName("QuickEd");
    QApplication::setQuitOnLastWindowClosed(false);

    DAVA::Core::Run( argc, argv );
    new DAVA::QtLayer();
    InitPVRTexTool();
    DAVA::ParticleEmitter::FORCE_DEEP_CLONE = true;

    auto loopManager = new DavaLoop();
    auto loop = new FrameworkLoop();

    auto *editorCore = new EditorCore();
    auto mainWindow = editorCore->GetMainWindow();
    auto glWidget = mainWindow->GetGLWidget();

    loop->SetOpenGLWindow(glWidget);
    editorCore->Start();
    loopManager->StartLoop( loop );

    QApplication::exec();

    glWidget->setParent(nullptr);

    delete editorCore;
    delete loop;
    delete loopManager;
    DAVA::QtLayer::Instance()->Release();
    // TODO: fix crash on release
    // DAVA::Core::Instance()->Release();    

    delete glWidget;

    return 0;
}
