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


#include "VirtualCoordinatesSystem.h"
#include "RenderSystem2D.h"
#include "UI/UIControlSystem.h"
#include "Render/2D/TextBlock.h"
#include "Render/2D/Sprite.h"

namespace DAVA
{
VirtualCoordinatesSystem::VirtualCoordinatesSystem()
{
    fixedProportions = true;
    enabledReloadResourceOnResize = false;
    wasScreenResized = false;

    desirableIndex = 0;

    virtualToPhysical = 1.f;
    physicalToVirtual = 1.f;

    EnableReloadResourceOnResize(true);
}

void VirtualCoordinatesSystem::ScreenSizeChanged()
{
    // on android sometime we catch FPU devide by 0 and several
    // calls to this method so here is simple shield
    if (physicalScreenSize.dx == 0 || physicalScreenSize.dy == 0)
    {
        Logger::Error("[VirtualCoordinatesSystem::ScreenSizeChanged] physicalScreenSize.dx == 0 || physicalScreenSize.dy == 0");
        return;
    }
    if (virtualScreenSize.dx == 0 || virtualScreenSize.dy == 0)
    {
        Logger::Error("[VirtualCoordinatesSystem::ScreenSizeChanged] physicalScreenSize.dx == 0 || physicalScreenSize.dy == 0");
        return;
    }
    wasScreenResized = false;

    virtualScreenSize = requestedVirtualScreenSize;

    float32 w, h;
    drawOffset = Vector2();
    w = float32(virtualScreenSize.dx) / float32(physicalScreenSize.dx);
    h = float32(virtualScreenSize.dy) / float32(physicalScreenSize.dy);
    float32 desD = 10000.0f;
    if (w > h)
    {
        physicalToVirtual = w;
        virtualToPhysical = float32(physicalScreenSize.dx) / float32(virtualScreenSize.dx);
        if (fixedProportions)
        {
            drawOffset.y = 0.5f * (float32(physicalScreenSize.dy) - float32(virtualScreenSize.dy) * virtualToPhysical);
        }
        else
        {
            virtualScreenSize.dy = Round(physicalScreenSize.dy * physicalToVirtual);
        }

        for (int32 i = 0; i < int32(allowedSizes.size()); i++)
        {
            allowedSizes[i].toVirtual = float32(virtualScreenSize.dx) / float32(allowedSizes[i].width);
            allowedSizes[i].toPhysical = float32(physicalScreenSize.dx) / float32(allowedSizes[i].width);
            if (fabs(allowedSizes[i].toPhysical - 1.0f) < desD)
            {
                desD = fabsf(allowedSizes[i].toPhysical - 1.0f);
                desirableIndex = i;
            }
        }
    }
    else
    {
        physicalToVirtual = h;
        virtualToPhysical = float32(physicalScreenSize.dy) / float32(virtualScreenSize.dy);
        if (fixedProportions)
        {
            drawOffset.x = 0.5f * (physicalScreenSize.dx - virtualScreenSize.dx * virtualToPhysical);
        }
        else
        {
            virtualScreenSize.dx = Round(physicalScreenSize.dx * physicalToVirtual);
        }

        for (int32 i = 0; i < int32(allowedSizes.size()); i++)
        {
            allowedSizes[i].toVirtual = virtualScreenSize.dy / float32(allowedSizes[i].height);
            allowedSizes[i].toPhysical = physicalScreenSize.dy / float32(allowedSizes[i].height);
            if (fabs(allowedSizes[i].toPhysical - 1.0f) < desD)
            {
                desD = fabsf(allowedSizes[i].toPhysical - 1.0f);
                desirableIndex = i;
            }
        }
    }

    drawOffset.y = floorf(drawOffset.y);
    drawOffset.x = floorf(drawOffset.x);

    fullVirtualScreenRect = Rect(-Round(drawOffset.x * physicalToVirtual),
                                 -Round(drawOffset.y * physicalToVirtual),
                                 Round((physicalScreenSize.dx - 2.f * drawOffset.x) * physicalToVirtual),
                                 Round((physicalScreenSize.dy - 2.f * drawOffset.y) * physicalToVirtual)
                                 );

    w = virtualScreenSize.dx / float32(inputAreaSize.dx);
    h = virtualScreenSize.dy / float32(inputAreaSize.dy);
    inputOffset.x = inputOffset.y = 0;
    if (w > h)
    {
        inputScaleFactor = w;
        inputOffset.y = 0.5f * (virtualScreenSize.dy - inputAreaSize.dy * inputScaleFactor);
    }
    else
    {
        inputScaleFactor = h;
        inputOffset.x = 0.5f * (virtualScreenSize.dx - inputAreaSize.dx * inputScaleFactor);
    }

    virtualSizeChanged.Emit(virtualScreenSize);

    if (enabledReloadResourceOnResize)
    {
        Sprite::ValidateForSize();
        TextBlock::ScreenResolutionChanged();
    }

    RenderSystem2D::Instance()->ScreenSizeChanged();
    UIControlSystem::Instance()->ScreenSizeChanged(GetFullScreenVirtualRect());
}

void VirtualCoordinatesSystem::EnableReloadResourceOnResize(bool enable)
{
    enabledReloadResourceOnResize = enable;
}

void VirtualCoordinatesSystem::SetPhysicalScreenSize(int32 width, int32 height)
{
    physicalScreenSize.dx = width;
    physicalScreenSize.dy = height;
    wasScreenResized = true;

    physicalSizeChanged.Emit(physicalScreenSize);
}

void VirtualCoordinatesSystem::SetVirtualScreenSize(float32 width, float32 height)
{
    requestedVirtualScreenSize.dx = virtualScreenSize.dx = width;
    requestedVirtualScreenSize.dy = virtualScreenSize.dy = height;
    wasScreenResized = true;
}

void VirtualCoordinatesSystem::SetInputScreenAreaSize(float32 width, float32 height)
{
    inputAreaSize.dx = width;
    inputAreaSize.dy = height;
    wasScreenResized = true;

    inputAreaSizeChanged.Emit(inputAreaSize);
}

void VirtualCoordinatesSystem::SetProportionsIsFixed(bool needFixed)
{
    fixedProportions = needFixed;
    wasScreenResized = true;
}

void VirtualCoordinatesSystem::RegisterAvailableResourceSize(float32 width, float32 height, const String& resourcesFolderName)
{
    VirtualCoordinatesSystem::ResourceSpaceSize newSize;
    newSize.width = width;
    newSize.height = height;
    newSize.folderName = resourcesFolderName;

    allowedSizes.push_back(newSize);
}

void VirtualCoordinatesSystem::UnregisterAllAvailableResourceSizes()
{
    allowedSizes.clear();
}
};
