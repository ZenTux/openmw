#include "localmap.hpp"
#include "renderingmanager.hpp"

#include <OgreOverlayManager.h>
#include <OgreMaterialManager.h>

using namespace MWRender;
using namespace Ogre;

#define MAP_RESOLUTION 1024 // 1024*1024 pixels for a SIZE*SIZE area in world units

// warning: don't set this too high! dynamic textures are a bottleneck
#define FOGOFWAR_RESOLUTION 32

// how many frames to skip before rendering the fog of war.
// example: at 60 fps, a value of 2 would mean to render it at 20 fps.
#define FOGOFWAR_SKIP 2

// size of a map segment (for exterior regions, this equals 1 cell)
#define SIZE 8192.f

LocalMap::LocalMap(OEngine::Render::OgreRenderer* rend)
{
    mRendering = rend;

    mCellCamera = mRendering->getScene()->createCamera("CellCamera");
    mCellCamera->setProjectionType(PT_ORTHOGRAPHIC);
    // look down -y
    const float sqrt0pt5 = 0.707106781;
    mCellCamera->setOrientation(Quaternion(sqrt0pt5, -sqrt0pt5, 0, 0));

    // Debug overlay to view the maps

    render(0, 0, 10000, 10000, SIZE, SIZE, "Cell_0_0_");

    MaterialPtr mat = MaterialManager::getSingleton().create("testMaterial", ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
    mat->getTechnique(0)->getPass(0)->createTextureUnitState("Cell_0_0_");
    mat->getTechnique(0)->getPass(0)->setDepthWriteEnabled(false);
    mat->getTechnique(0)->getPass(0)->setSceneBlending(SBT_TRANSPARENT_ALPHA);
    mat->getTechnique(0)->getPass(0)->setDepthCheckEnabled(false);

    mat = MaterialManager::getSingleton().create("testMaterial2", ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
    mat->getTechnique(0)->getPass(0)->createTextureUnitState("Cell_0_0_");
    mat->getTechnique(0)->getPass(0)->setDepthWriteEnabled(false);
    mat->getTechnique(0)->getPass(0)->setSceneBlending(SBT_TRANSPARENT_ALPHA);
    mat->getTechnique(0)->getPass(0)->setDepthCheckEnabled(false);
    
    OverlayManager& ovm = OverlayManager::getSingleton();

    Overlay* mOverlay = ovm.create( "testOverlay" );
    mOverlay->setZOrder(0);
    OverlayContainer* overlay_panel;
    overlay_panel = (OverlayContainer*)ovm.createOverlayElement("Panel", "testPanel");
    
    overlay_panel->_setPosition(0, 0);
    overlay_panel->_setDimensions(0.5, 0.5);
    
    overlay_panel->setMaterialName( "testMaterial" );
    overlay_panel->show();
    mOverlay->add2D(overlay_panel);
    mOverlay->show();

    Overlay* mOverlay2 = ovm.create( "testOverlay2" );
    mOverlay2->setZOrder(1);
    OverlayContainer* overlay_panel2;
    overlay_panel2 = (OverlayContainer*)ovm.createOverlayElement("Panel", "testPanel2");
    
    overlay_panel2->_setPosition(0, 0);
    overlay_panel2->_setDimensions(0.5, 0.5);
    
    overlay_panel2->setMaterialName( "testMaterial2" );
    overlay_panel2->show();
    mOverlay2->add2D(overlay_panel2);
    
    mOverlay2->show();
    
}

LocalMap::~LocalMap()
{
    deleteBuffers();
}

void LocalMap::deleteBuffers()
{
    for (std::map<std::string, uint32*>::iterator it=mBuffers.begin();
        it != mBuffers.end(); ++it)
    {
        delete it->second;
    }
    mBuffers.clear();
}

void LocalMap::saveTexture(const std::string& texname, const std::string& filename)
{
    TexturePtr tex = TextureManager::getSingleton().getByName(texname);
    if (tex.isNull()) return;
    HardwarePixelBufferSharedPtr readbuffer = tex->getBuffer();
    readbuffer->lock(HardwareBuffer::HBL_NORMAL );
    const PixelBox &readrefpb = readbuffer->getCurrentLock();    
    uchar *readrefdata = static_cast<uchar*>(readrefpb.data);        

    Image img;
    img = img.loadDynamicImage (readrefdata, tex->getWidth(),
        tex->getHeight(), tex->getFormat());    
    img.save("./" + filename);

    readbuffer->unlock();
}

std::string LocalMap::coordStr(const int x, const int y)
{
    return StringConverter::toString(x) + "_" + StringConverter::toString(y);
}

void LocalMap::saveFogOfWar(MWWorld::Ptr::CellStore* cell)
{
    if (!mInterior)
    {
        /*saveTexture("Cell_"+coordStr(mCellX, mCellY)+"_fog",
            "Cell_"+coordStr(mCellX, mCellY)+"_fog.png");*/
    }
    else
    {
        Vector2 min(mBounds.getMinimum().x, mBounds.getMinimum().z);
        Vector2 max(mBounds.getMaximum().x, mBounds.getMaximum().z);
        /// \todo why is this workaround needed?
        min *= 1.3;
        max *= 1.3;
        Vector2 length = max-min;
        
        // divide into segments
        const int segsX = std::ceil( length.x / SIZE );
        const int segsY = std::ceil( length.y / SIZE );

        for (int x=0; x<segsX; ++x)
        {
            for (int y=0; y<segsY; ++y)
            {
                /*saveTexture(
                    mInteriorName + "_" + coordStr(x,y) + "_fog",
                    mInteriorName + "_" + coordStr(x,y) + "_fog.png");*/
            }
        }
    }
}

void LocalMap::requestMap(MWWorld::Ptr::CellStore* cell)
{
    mInterior = false;

    std::string name = "Cell_"+coordStr(cell->cell->data.gridX, cell->cell->data.gridY);

    int x = cell->cell->data.gridX;
    int y = cell->cell->data.gridY;

    render((x+0.5)*SIZE, (-y-0.5)*SIZE, -10000, 10000, SIZE, SIZE, name);
}

void LocalMap::requestMap(MWWorld::Ptr::CellStore* cell,
                            AxisAlignedBox bounds)
{
    mInterior = true;
    mBounds = bounds;
    
    Vector2 z(bounds.getMaximum().y, bounds.getMinimum().y);
    Vector2 min(bounds.getMinimum().x, bounds.getMinimum().z);
    Vector2 max(bounds.getMaximum().x, bounds.getMaximum().z);

    /// \todo why is this workaround needed?
    min *= 1.3;
    max *= 1.3;

    Vector2 length = max-min;
    Vector2 center(bounds.getCenter().x, bounds.getCenter().z);

    // divide into segments
    const int segsX = std::ceil( length.x / SIZE );
    const int segsY = std::ceil( length.y / SIZE );

    mInteriorName = cell->cell->name;

    for (int x=0; x<segsX; ++x)
    {
        for (int y=0; y<segsY; ++y)
        {
            Vector2 start = min + Vector2(SIZE*x,SIZE*y);
            Vector2 newcenter = start + 4096;

            render(newcenter.x, newcenter.y, z.y, z.x, SIZE, SIZE,
                cell->cell->name + "_" + coordStr(x,y));
        }
    }
}

void LocalMap::render(const float x, const float y,
                    const float zlow, const float zhigh,
                    const float xw, const float yw, const std::string& texture)
{
    // disable fog
    // changing FOG_MODE is not a solution when using shaders, thus we have to push linear start/end
    const float fStart = mRendering->getScene()->getFogStart();
    const float fEnd = mRendering->getScene()->getFogEnd();
    const ColourValue& clr = mRendering->getScene()->getFogColour();
    mRendering->getScene()->setFog(FOG_LINEAR, clr, 0, 1000000, 10000000);

    // make everything visible
    mRendering->getScene()->setAmbientLight(ColourValue(1,1,1));

    mCellCamera->setPosition(Vector3(x, zhigh, y));
    mCellCamera->setFarClipDistance( (zhigh-zlow) * 1.1 );

    mCellCamera->setOrthoWindow(xw, yw);

    TexturePtr tex;
    // try loading from memory
    tex = TextureManager::getSingleton().getByName(texture);
    if (tex.isNull())
    {
        // try loading from disk
        //if (boost::filesystem::exists(texture+".jpg"))
        //{
            /// \todo
        //}
        //else
        {
            // render
            tex = TextureManager::getSingleton().createManual(
                            texture,
                            ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
                            TEX_TYPE_2D,
                            xw*MAP_RESOLUTION/SIZE, yw*MAP_RESOLUTION/SIZE, 
                            0,
                            PF_R8G8B8,
                            TU_RENDERTARGET);

            RenderTarget* rtt = tex->getBuffer()->getRenderTarget();
            rtt->setAutoUpdated(false);
            Viewport* vp = rtt->addViewport(mCellCamera);
            vp->setOverlaysEnabled(false);
            vp->setShadowsEnabled(false);
            vp->setBackgroundColour(ColourValue(0, 0, 0));
            //vp->setVisibilityMask( ... );

            rtt->update();

            // create "fog of war" texture
            TexturePtr tex2 = TextureManager::getSingleton().createManual(
                            texture + "_fog",
                            ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
                            TEX_TYPE_2D,
                            xw*FOGOFWAR_RESOLUTION/SIZE, yw*FOGOFWAR_RESOLUTION/SIZE, 
                            0,
                            PF_A8R8G8B8,
                            TU_DYNAMIC_WRITE_ONLY);

            // create a buffer to use for dynamic operations
            uint32* buffer = new uint32[FOGOFWAR_RESOLUTION*FOGOFWAR_RESOLUTION];

            // initialize to (0, 0, 0, 1)
            uint32* pointer = buffer;
            for (int p=0; p<FOGOFWAR_RESOLUTION*FOGOFWAR_RESOLUTION; ++p)
            {
                *(pointer+p) = (255 << 24);
            }

            memcpy(tex2->getBuffer()->lock(HardwareBuffer::HBL_DISCARD), buffer, FOGOFWAR_RESOLUTION*FOGOFWAR_RESOLUTION*4);
            tex2->getBuffer()->unlock();

            mBuffers[texture] = buffer;

            // save to cache for next time
            //rtt->writeContentsToFile("./" + texture + ".jpg");
        }
    }
    

    // re-enable fog
    mRendering->getScene()->setFog(FOG_LINEAR, clr, 0, fStart, fEnd);
}

void LocalMap::setPlayerPosition (const Ogre::Vector3& position)
{
    #if FOGOFWAR_SKIP != 0
    static int count=0;
    if (++count % FOGOFWAR_SKIP != 0)
        return;
    #endif

    // retrieve the x,y grid coordinates the player is in 
    int x,y;
    Vector2 pos(position.x, position.z);
    if (!mInterior)
    {
        x = std::ceil(pos.x / SIZE)-1;
        y = std::ceil(-pos.y / SIZE)-1;
        mCellX = x;
        mCellY = y;
    }
    else
    {
        Vector2 min(mBounds.getMinimum().x, mBounds.getMinimum().z);
        min *= 1.3;
        
        x = std::ceil((pos.x - min.x)/SIZE)-1;
        y = std::ceil((pos.y - min.y)/SIZE)-1;
    }

    // convert from world coordinates to texture UV coordinates
    float u,v;
    std::string texName;
    if (!mInterior)
    {
        u = std::abs((pos.x - (SIZE*x))/SIZE);
        v = 1-std::abs((pos.y + (SIZE*y))/SIZE);
        texName = "Cell_"+coordStr(x,y);
    }
    else
    {
        Vector2 min(mBounds.getMinimum().x, mBounds.getMinimum().z);
        min *= 1.3;

        u = (pos.x - min.x - SIZE*x)/SIZE;
        v = (pos.y - min.y - SIZE*y)/SIZE;

        texName = mInteriorName + "_" + coordStr(x,y);
    }

    // explore radius (squared)
    const float sqrExploreRadius = 0.01 * FOGOFWAR_RESOLUTION*FOGOFWAR_RESOLUTION;

    // get the appropriate fog of war texture
    TexturePtr tex = TextureManager::getSingleton().getByName(texName+"_fog");
    if (!tex.isNull())
    {
        // get its buffer
        if (mBuffers.find(texName) == mBuffers.end()) return;
        uint32* buffer = mBuffers[texName];
        uint32* pointer = buffer;
        for (int texV = 0; texV<FOGOFWAR_RESOLUTION; ++texV)
        {
            for (int texU = 0; texU<FOGOFWAR_RESOLUTION; ++texU)
            {
                float sqrDist = Math::Sqr(texU - u*FOGOFWAR_RESOLUTION) + Math::Sqr(texV - v*FOGOFWAR_RESOLUTION);
                uint32 clr = *pointer;
                uint8 alpha = (clr >> 24);
                alpha = std::min( alpha, (uint8) (std::max(0.f, std::min(1.f, (sqrDist/sqrExploreRadius)))*255) );
                *((uint32*)pointer) = (alpha << 24);

                // move to next texel
                ++pointer;
            }
        }

        // copy to the texture
        memcpy(tex->getBuffer()->lock(HardwareBuffer::HBL_DISCARD), buffer, FOGOFWAR_RESOLUTION*FOGOFWAR_RESOLUTION*4);
        tex->getBuffer()->unlock();

        if (!MaterialManager::getSingleton().getByName("testMaterial").isNull())
        {
            MaterialPtr mat = MaterialManager::getSingleton().getByName("testMaterial");
            mat->getTechnique(0)->getPass(0)->getTextureUnitState(0)->setTextureName(texName);
        }
        if (!MaterialManager::getSingleton().getByName("testMaterial2").isNull())
        {
            MaterialPtr mat = MaterialManager::getSingleton().getByName("testMaterial2");
            mat->getTechnique(0)->getPass(0)->getTextureUnitState(0)->setTextureName(tex->getName());
        }
        
    }
}
