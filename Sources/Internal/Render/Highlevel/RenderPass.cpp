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


#include "Render/Highlevel/RenderPass.h"
#include "Render/Highlevel/RenderLayer.h"
#include "Render/Highlevel/RenderBatchArray.h"
#include "Render/Highlevel/Camera.h"
#include "Render/Highlevel/RenderPassNames.h"
#include "Render/Highlevel/ShadowVolumeRenderLayer.h"
#include "Render/ShaderCache.h"

#include "Render/Renderer.h"
#include "Render/Texture.h"

#include "Render/Image/ImageSystem.h"

namespace DAVA
{

RenderPass::RenderPass(const FastName & _name) : passName(_name)
{
    renderLayers.reserve(RenderLayer::RENDER_LAYER_ID_COUNT);
    
    passConfig.colorBuffer[0].loadAction = rhi::LOADACTION_CLEAR;
    passConfig.colorBuffer[0].storeAction = rhi::STOREACTION_NONE;
    passConfig.colorBuffer[0].clearColor[0] = 0.25f;
    passConfig.colorBuffer[0].clearColor[1] = 0.25f;
    passConfig.colorBuffer[0].clearColor[2] = 0.35f;
    passConfig.colorBuffer[0].clearColor[3] = 1.0f;
    passConfig.depthStencilBuffer.loadAction = rhi::LOADACTION_CLEAR;
    passConfig.depthStencilBuffer.storeAction = rhi::STOREACTION_NONE;
}

RenderPass::~RenderPass()
{
    ClearLayersArrays();
    for (RenderLayer * layer : renderLayers)
        SafeDelete(layer);
}
    
void RenderPass::AddRenderLayer(RenderLayer * layer, RenderLayer::eRenderLayerID afterLayer)
{
    if (RenderLayer::RENDER_LAYER_INVALID_ID != afterLayer)
	{
		uint32 size = static_cast<uint32>(renderLayers.size());
		for(uint32 i = 0; i < size; ++i)
		{
            RenderLayer::eRenderLayerID layerID = renderLayers[i]->GetRenderLayerID();
            if(afterLayer == layerID)
			{
				renderLayers.insert(renderLayers.begin() + i + 1, layer);
                layersBatchArrays[layerID].SetSortingFlags(layer->GetSortingFlags());
				return;
			}
		}
		DVASSERT(0 && "RenderPass::AddRenderLayer afterLayer not found");
	}
	else
	{
        renderLayers.push_back(layer);
        layersBatchArrays[layer->GetRenderLayerID()].SetSortingFlags(layer->GetSortingFlags());
	}
}
    
void RenderPass::RemoveRenderLayer(RenderLayer * layer)
{
	Vector<RenderLayer*>::iterator it = std::find(renderLayers.begin(), renderLayers.end(), layer);
	DVASSERT(it != renderLayers.end());

	renderLayers.erase(it);
}

void RenderPass::Draw(RenderSystem * renderSystem, uint32 clearBuffers)
{   
    Camera *mainCamera = renderSystem->GetMainCamera();        
    Camera *drawCamera = renderSystem->GetDrawCamera();   
    
    DVASSERT(drawCamera);
    DVASSERT(mainCamera);
    drawCamera->SetupDynamicParameters();            
    if (mainCamera!=drawCamera)    
        mainCamera->PrepareDynamicParameters();
    
    PrepareVisibilityArrays(mainCamera, renderSystem);
    
    //ClearBuffers(clearBuffers);

    DrawLayers(mainCamera);
}

void RenderPass::PrepareVisibilityArrays(Camera *camera, RenderSystem * renderSystem)
{
    uint32 currVisibilityCriteria = RenderObject::CLIPPING_VISIBILITY_CRITERIA;
    if (!Renderer::GetOptions()->IsOptionEnabled(RenderOptions::ENABLE_STATIC_OCCLUSION))
        currVisibilityCriteria&=~RenderObject::VISIBLE_STATIC_OCCLUSION;

    visibilityArray.clear();
    renderSystem->GetRenderHierarchy()->Clip(camera, visibilityArray, currVisibilityCriteria);

    ClearLayersArrays();
    PrepareLayersArrays(visibilityArray, camera);
}

void RenderPass::PrepareLayersArrays(const Vector<RenderObject *> objectsArray, Camera * camera)
{
    uint32 size = objectsArray.size();
    for (uint32 ro = 0; ro < size; ++ro)
    {
        RenderObject * renderObject = objectsArray[ro];
        if (renderObject->GetFlags() & RenderObject::CUSTOM_PREPARE_TO_RENDER)
        {
            renderObject->PrepareToRender(camera);
        }

        uint32 batchCount = renderObject->GetActiveRenderBatchCount();
        for (uint32 batchIndex = 0; batchIndex < batchCount; ++batchIndex)
        {
            RenderBatch * batch = renderObject->GetActiveRenderBatch(batchIndex);

            NMaterial * material = batch->GetMaterial();
            DVASSERT(material);
            if (material->PreBuildMaterial(passName))
            {
                layersBatchArrays[material->GetRenderLayerID()].AddRenderBatch(batch);
            }
        }
    }
}

void RenderPass::DrawLayers(Camera *camera)
{    
    //ShaderCache::Instance()->ClearAllLastBindedCaches();        

    rhi::HPacketList pl;
    rhi::HRenderPass pass = rhi::AllocateRenderPass(passConfig, 1, &pl);
    rhi::BeginRenderPass(pass);
    rhi::BeginPacketList(pl);

    // Draw all layers with their materials
    uint32 size = (uint32)renderLayers.size();
    for (uint32 k = 0; k < size; ++k)
    {
        RenderLayer * layer = renderLayers[k];
        RenderBatchArray & batchArray = layersBatchArrays[layer->GetRenderLayerID()];

        batchArray.Sort(camera);

        layer->Draw(camera, batchArray, pl);
    }
    rhi::EndPacketList(pl);
    rhi::EndRenderPass(pass);
}

void RenderPass::ClearLayersArrays()
{
    for (uint32 id = 0; id < (uint32)RenderLayer::RENDER_LAYER_ID_COUNT; ++id)
    {
        layersBatchArrays[id].Clear();
    }
}

MainForwardRenderPass::MainForwardRenderPass(const FastName & name) : RenderPass(name),
    reflectionPass(NULL),
    refractionPass(NULL),
    reflectionTexture(NULL),
    refractionTexture(NULL),
    needWaterPrepass(false)
{
    AddRenderLayer(new RenderLayer(RenderLayer::RENDER_LAYER_OPAQUE_ID, RenderLayer::LAYER_SORTING_FLAGS_OPAQUE));
    AddRenderLayer(new RenderLayer(RenderLayer::RENDER_LAYER_AFTER_OPAQUE_ID, RenderLayer::LAYER_SORTING_FLAGS_AFTER_OPAQUE));
    AddRenderLayer(new RenderLayer(RenderLayer::RENDER_LAYER_VEGETATION_ID, RenderLayer::LAYER_SORTING_FLAGS_VEGETATION));
    AddRenderLayer(new RenderLayer(RenderLayer::RENDER_LAYER_ALPHA_TEST_LAYER_ID, RenderLayer::LAYER_SORTING_FLAGS_ALPHA_TEST_LAYER));
    AddRenderLayer(new ShadowVolumeRenderLayer(RenderLayer::RENDER_LAYER_SHADOW_VOLUME_ID, RenderLayer::LAYER_SORTING_FLAGS_SHADOW_VOLUME));
    AddRenderLayer(new RenderLayer(RenderLayer::RENDER_LAYER_WATER_ID, RenderLayer::LAYER_SORTING_FLAGS_WATER));
    AddRenderLayer(new RenderLayer(RenderLayer::RENDER_LAYER_TRANSLUCENT_ID, RenderLayer::LAYER_SORTING_FLAGS_TRANSLUCENT));
    AddRenderLayer(new RenderLayer(RenderLayer::RENDER_LAYER_AFTER_TRANSLUCENT_ID, RenderLayer::LAYER_SORTING_FLAGS_AFTER_TRANSLUCENT));
    AddRenderLayer(new RenderLayer(RenderLayer::RENDER_LAYER_DEBUG_DRAW_ID, RenderLayer::LAYER_SORTING_FLAGS_DEBUG_DRAW));
}


void MainForwardRenderPass::PrepareReflectionRefractionTextures(RenderSystem * renderSystem)
{
#if RHI_COMPLETE
    if (!Renderer::GetOptions()->IsOptionEnabled(RenderOptions::WATER_REFLECTION_REFRACTION_DRAW))
        return;

    const static int32 REFLECTION_TEX_SIZE = 512;
    const static int32 REFRACTION_TEX_SIZE = 512;
    if (!reflectionPass)
    {             
        reflectionPass = new WaterReflectionRenderPass(PASS_FORWARD);
        reflectionTexture = Texture::CreateFBO(REFLECTION_TEX_SIZE, REFLECTION_TEX_SIZE, FORMAT_RGB565, Texture::DEPTH_RENDERBUFFER);          
                    
        refractionPass = new WaterRefractionRenderPass(PASS_FORWARD);
        refractionTexture = Texture::CreateFBO(REFRACTION_TEX_SIZE, REFRACTION_TEX_SIZE, FORMAT_RGB565, Texture::DEPTH_RENDERBUFFER);                  
    }   

    Rect viewportSave = RenderManager::Instance()->GetViewport();
    Texture * renderTargetSave = RenderManager::Instance()->GetRenderTarget();
        
    RenderManager::Instance()->SetRenderTarget(reflectionTexture);
    //discard everything here
    RenderManager::Instance()->SetViewport(Rect(0, 0, (float32)REFLECTION_TEX_SIZE, (float32)REFLECTION_TEX_SIZE));

    reflectionPass->SetWaterLevel(waterBox.max.z);
    reflectionPass->Draw(renderSystem, RenderManager::ALL_BUFFERS);

        
    //discrad depth(everything?) here
    RenderManager::Instance()->DiscardFramebufferHW(RenderManager::DEPTH_ATTACHMENT|RenderManager::STENCIL_ATTACHMENT);
        
        
    RenderManager::Instance()->SetRenderTarget(refractionTexture);
        
    RenderManager::Instance()->SetViewport(Rect(0, 0, (float32)REFLECTION_TEX_SIZE, (float32)REFLECTION_TEX_SIZE));

    refractionPass->SetWaterLevel(waterBox.min.z);
    refractionPass->Draw(renderSystem, RenderManager::ALL_BUFFERS);

    //discrad depth(everything?) here
    RenderManager::Instance()->DiscardFramebufferHW(RenderManager::DEPTH_ATTACHMENT|RenderManager::STENCIL_ATTACHMENT);

    RenderManager::Instance()->SetRenderTarget(renderTargetSave);
    RenderManager::Instance()->SetViewport(viewportSave);

    renderSystem->GetDrawCamera()->SetupDynamicParameters();    		
        
#endif RHI_COMPLETE
}

void MainForwardRenderPass::Draw(RenderSystem * renderSystem, uint32 clearBuffers)
{
    Camera *mainCamera = renderSystem->GetMainCamera();        
    Camera *drawCamera = renderSystem->GetDrawCamera();   
    DVASSERT(mainCamera);
    DVASSERT(drawCamera);
    drawCamera->SetupDynamicParameters();            
    if (mainCamera!=drawCamera)    
        mainCamera->PrepareDynamicParameters();

    if (needWaterPrepass)
    {
        /*water presence is cached from previous frame in optimization purpose*/
        /*if on previous frame there was water - reflection and refraction textures are rendered first (it helps to avoid excessive renderPassBatchArray->PrepareVisibilityArray)*/
        /* if there was no water on previous frame, and it appears on this frame - reflection and refractions textures are still to be rendered*/
        PrepareReflectionRefractionTextures(renderSystem);
    }
    
	//important: FoliageSystem also using main camera for cliping vegetation cells
    PrepareVisibilityArrays(mainCamera, renderSystem);
	
    const RenderBatchArray & waterLayerBatches = layersBatchArrays[RenderLayer::RENDER_LAYER_WATER_ID];
    uint32 waterBatchesCount = 0;//waterLayer->GetRenderBatchCount();	#if RHI_COMPLETE
	if (waterBatchesCount)
	{        
        waterBox.Empty();
		for (uint32 i=0; i<waterBatchesCount; ++i)
		{
            RenderBatch *batch = waterLayerBatches.Get(i);
			waterBox.AddAABBox(batch->GetRenderObject()->GetWorldBoundingBox());
			
		}
	}    
    
	if (!needWaterPrepass&&waterBatchesCount)
	{
        PrepareReflectionRefractionTextures(renderSystem); 
        /*as PrepareReflectionRefractionTextures builds render batches according to reflection/refraction camera - render batches in main pass list are not valid anymore*/
        /*to avoid this happening every frame water visibility is cached from previous frame (needWaterPrepass)*/
        /*however if there was no water on previous frame and there is water on this frame visibilityArray should be re-prepared*/
        ClearLayersArrays();
        PrepareLayersArrays(visibilityArray, mainCamera);
	}	
    needWaterPrepass = (waterBatchesCount!=0); //for next frame;    
    //ClearBuffers(clearBuffers);
#if RHI_COMPLETE
    Rect viewportSave = RenderManager::Instance()->GetViewport();
    Vector2 rssVal(1.0f / viewportSave.dx, 1.0f / viewportSave.dy);
    Vector2 screenOffsetVal(viewportSave.x, viewportSave.y);
    for (uint32 i = 0; i < waterBatchesCount; ++i)
    {
        NMaterial *mat = waterLayer->Get(i)->GetMaterial();
        mat->SetPropertyValue(NMaterial::PARAM_RCP_SCREEN_SIZE, Shader::UT_FLOAT_VEC2, 1, &rssVal);
        mat->SetPropertyValue(NMaterial::PARAM_SCREEN_OFFSET, Shader::UT_FLOAT_VEC2, 1, &screenOffsetVal);
        mat->SetTexture(NMaterial::TEXTURE_DYNAMIC_REFLECTION, reflectionTexture);
        mat->SetTexture(NMaterial::TEXTURE_DYNAMIC_REFRACTION, refractionTexture);
    }

    ClearBuffers(clearBuffers);
#endif //RHI_COMPLETE

	DrawLayers(mainCamera);   
}

MainForwardRenderPass::~MainForwardRenderPass()
{	
	SafeRelease(reflectionTexture);
	SafeRelease(refractionTexture);
	SafeDelete(reflectionPass);
	SafeDelete(refractionPass);
}

WaterPrePass::WaterPrePass(const FastName & name) : RenderPass(name), passMainCamera(NULL), passDrawCamera(NULL)
{
    AddRenderLayer(new RenderLayer(RenderLayer::RENDER_LAYER_OPAQUE_ID, RenderLayer::LAYER_SORTING_FLAGS_OPAQUE));
    AddRenderLayer(new RenderLayer(RenderLayer::RENDER_LAYER_AFTER_OPAQUE_ID, RenderLayer::LAYER_SORTING_FLAGS_AFTER_OPAQUE));
    AddRenderLayer(new RenderLayer(RenderLayer::RENDER_LAYER_ALPHA_TEST_LAYER_ID, RenderLayer::LAYER_SORTING_FLAGS_ALPHA_TEST_LAYER));
    AddRenderLayer(new RenderLayer(RenderLayer::RENDER_LAYER_TRANSLUCENT_ID, RenderLayer::LAYER_SORTING_FLAGS_TRANSLUCENT));
    AddRenderLayer(new RenderLayer(RenderLayer::RENDER_LAYER_AFTER_TRANSLUCENT_ID, RenderLayer::LAYER_SORTING_FLAGS_AFTER_TRANSLUCENT));
}
WaterPrePass::~WaterPrePass()
{
    SafeRelease(passMainCamera);
    SafeRelease(passDrawCamera);
}

WaterReflectionRenderPass::WaterReflectionRenderPass(const FastName & name) : WaterPrePass(name)
{	
}

void WaterReflectionRenderPass::UpdateCamera(Camera *camera)
{
    Vector3 v;
    v = camera->GetPosition();
    v.z = waterLevel - (v.z - waterLevel);
    camera->SetPosition(v);
    v = camera->GetTarget();
    v.z = waterLevel - (v.z - waterLevel);
    camera->SetTarget(v);        
}

void WaterReflectionRenderPass::Draw(RenderSystem * renderSystem, uint32 clearBuffers)
{    
    Camera *mainCamera = renderSystem->GetMainCamera();        
    Camera *drawCamera = renderSystem->GetDrawCamera();    
            

    if (!passDrawCamera)
    {
        passMainCamera = new Camera();    
        passDrawCamera = new Camera();            
    }

    passMainCamera->CopyMathOnly(*mainCamera);        
    UpdateCamera(passMainCamera);

    Vector4 clipPlane(0,0,1, -(waterLevel-0.1f));

    Camera* currMainCamera = passMainCamera;
    Camera* currDrawCamera;
    
    if (drawCamera==mainCamera)
    {
        currDrawCamera = currMainCamera;    
    }
    else
    {
        passDrawCamera->CopyMathOnly(*drawCamera);        
        UpdateCamera(passDrawCamera);
        currDrawCamera = passDrawCamera;
        currMainCamera->PrepareDynamicParameters(&clipPlane);
    }
    currDrawCamera->SetupDynamicParameters(&clipPlane);
    
    //add clipping plane
    
    
    
	visibilityArray.clear();
    renderSystem->GetRenderHierarchy()->Clip(currMainCamera, visibilityArray, RenderObject::CLIPPING_VISIBILITY_CRITERIA | RenderObject::VISIBLE_REFLECTION);

    ClearLayersArrays();
    PrepareLayersArrays(visibilityArray, currMainCamera);

    //ClearBuffers(clearBuffers);

    DrawLayers(currMainCamera);
}


WaterRefractionRenderPass::WaterRefractionRenderPass(const FastName & name) : WaterPrePass(name)
{
    /*const RenderLayerManager * renderLayerManager = RenderLayerManager::Instance();
    AddRenderLayer(renderLayerManager->GetRenderLayer(LAYER_SHADOW_VOLUME), LAST_LAYER);*/
}

void WaterRefractionRenderPass::Draw(RenderSystem * renderSystem, uint32 clearBuffers)
{
    Camera *mainCamera = renderSystem->GetMainCamera();        
    Camera *drawCamera = renderSystem->GetDrawCamera();    


    if (!passDrawCamera)
    {
        passMainCamera = new Camera();    
        passDrawCamera = new Camera();            
    }

    passMainCamera->CopyMathOnly(*mainCamera);                    

    Vector4 clipPlane(0,0,-1, waterLevel+0.1f);

    Camera* currMainCamera = passMainCamera;
    Camera* currDrawCamera;

    if (drawCamera==mainCamera)
    {
        currDrawCamera = currMainCamera;    
    }
    else
    {
        passDrawCamera->CopyMathOnly(*drawCamera);                
        currDrawCamera = passDrawCamera;
        currMainCamera->PrepareDynamicParameters(&clipPlane);
    }
    currDrawCamera->SetupDynamicParameters(&clipPlane);

    //add clipping plane



    visibilityArray.clear();
    renderSystem->GetRenderHierarchy()->Clip(currMainCamera, visibilityArray, RenderObject::CLIPPING_VISIBILITY_CRITERIA | RenderObject::VISIBLE_REFRACTION);

    ClearLayersArrays();
    PrepareLayersArrays(visibilityArray, currMainCamera);

    //ClearBuffers(clearBuffers);

    DrawLayers(currMainCamera);       
    
}


};
