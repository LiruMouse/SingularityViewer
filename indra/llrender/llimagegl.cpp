/** 
 * @file llimagegl.cpp
 * @brief Generic GL image handler
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */


// TODO: create 2 classes for images w/ and w/o discard levels?

#include "linden_common.h"

#include "llimagegl.h"

#include "llerror.h"
#include "llimage.h"

#include "llmath.h"
#include "llgl.h"
#include "llglslshader.h"
#include "llrender.h"

//----------------------------------------------------------------------------
const F32 MIN_TEXTURE_LIFETIME = 10.f;

//which power of 2 is i?
//assumes i is a power of 2 > 0
U32 wpo2(U32 i);

//statics

U32 LLImageGL::sUniqueCount				= 0;
U32 LLImageGL::sBindCount				= 0;
S64 LLImageGL::sGlobalTextureMemoryInBytes		= 0;
S64 LLImageGL::sBoundTextureMemoryInBytes		= 0;
S32 LLImageGL::sCurBoundTextureMemory	= 0;
S32 LLImageGL::sCount					= 0;

BOOL LLImageGL::sGlobalUseAnisotropic	= FALSE;
F32 LLImageGL::sLastFrameTime			= 0.f;
BOOL LLImageGL::sAllowReadBackRaw       = FALSE ;
LLImageGL* LLImageGL::sDefaultGLTexture = NULL ;
bool LLImageGL::sCompressTextures = false;

std::set<LLImageGL*> LLImageGL::sImageList;

//****************************************************************************************************
//The below for texture auditing use only
//****************************************************************************************************
//-----------------------
//debug use
BOOL gAuditTexture = FALSE ;
#define MAX_TEXTURE_LOG_SIZE 22 //2048 * 2048
std::vector<S32> LLImageGL::sTextureLoadedCounter(MAX_TEXTURE_LOG_SIZE + 1) ;
std::vector<S32> LLImageGL::sTextureBoundCounter(MAX_TEXTURE_LOG_SIZE + 1) ;
std::vector<S32> LLImageGL::sTextureCurBoundCounter(MAX_TEXTURE_LOG_SIZE + 1) ;
S32 LLImageGL::sCurTexSizeBar = -1 ;
S32 LLImageGL::sCurTexPickSize = -1 ;
LLPointer<LLImageGL> LLImageGL::sHighlightTexturep = NULL;
S32 LLImageGL::sMaxCategories = 1 ;

std::vector<S32> LLImageGL::sTextureMemByCategory;
std::vector<S32> LLImageGL::sTextureMemByCategoryBound ;
std::vector<S32> LLImageGL::sTextureCurMemByCategoryBound ;
//------------------------
// ****************************************************************************************************
//End for texture auditing use only
// ****************************************************************************************************

// **************************************************************************************
//below are functions for debug use
//do not delete them even though they are not currently being used.
void check_all_images()
{
	for (std::set<LLImageGL*>::iterator iter = LLImageGL::sImageList.begin();
		 iter != LLImageGL::sImageList.end(); iter++)
	{
		LLImageGL* glimage = *iter;
		if (glimage->getTexName() && glimage->isGLTextureCreated())
		{
			gGL.getTexUnit(0)->bind(glimage) ;
			glimage->checkTexSize() ;
			gGL.getTexUnit(0)->unbind(glimage->getTarget()) ;
		}
	}
}

void LLImageGL::checkTexSize(bool forced) const
{
	if ((forced || gDebugGL) && mTarget == GL_TEXTURE_2D)
	{
		{
			//check viewport
			GLint vp[4] ;
			glGetIntegerv(GL_VIEWPORT, vp) ;
			llcallstacks << "viewport: " << vp[0] << " : " << vp[1] << " : " << vp[2] << " : " << vp[3] << llcallstacksendl ;
		}

		GLint texname;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &texname);
		BOOL error = FALSE;
		if (texname != mTexName)
		{
			LL_INFOS() << "Bound: " << texname << " Should bind: " << mTexName << " Default: " << LLImageGL::sDefaultGLTexture->getTexName() << LL_ENDL;

			error = TRUE;
			if (gDebugSession)
			{
				gFailLog << "Invalid texture bound!" << std::endl;
			}
			else
			{
				LL_ERRS() << "Invalid texture bound!" << LL_ENDL;
			}
		}
		stop_glerror() ;
		LLGLint x = 0, y = 0 ;
		glGetTexLevelParameteriv(mTarget, 0, GL_TEXTURE_WIDTH, (GLint*)&x);
		glGetTexLevelParameteriv(mTarget, 0, GL_TEXTURE_HEIGHT, (GLint*)&y) ;
		stop_glerror() ;
		llcallstacks << "w: " << x << " h: " << y << llcallstacksendl ;

		if(!x || !y)
		{
			return ;
		}
		if(x != (mWidth >> mCurrentDiscardLevel) || y != (mHeight >> mCurrentDiscardLevel))
		{
			error = TRUE;
			if (gDebugSession)
			{
				gFailLog << "wrong texture size and discard level!" << 
					mWidth << " Height: " << mHeight << " Current Level: " << (S32)mCurrentDiscardLevel << std::endl;
			}
			else
			{
				LL_ERRS() << "wrong texture size and discard level: width: " << 
					mWidth << " Height: " << mHeight << " Current Level: " << (S32)mCurrentDiscardLevel << LL_ENDL ;
			}
		}

		if (error)
		{
			ll_fail("LLImageGL::checkTexSize failed.");
		}
	}
}
//end of debug functions
// **************************************************************************************

//----------------------------------------------------------------------------
BOOL is_little_endian()
{
	S32 a = 0x12345678;
    U8 *c = (U8*)(&a);
    
	return (*c == 0x78) ;
}
//static 
void LLImageGL::initClass(S32 num_catagories) 
{
	sMaxCategories = num_catagories ;

	sTextureMemByCategory.resize(sMaxCategories);
	sTextureMemByCategoryBound.resize(sMaxCategories) ;
	sTextureCurMemByCategoryBound.resize(sMaxCategories) ;
}

//static 
void LLImageGL::cleanupClass() 
{	
	sTextureMemByCategory.clear() ;
	sTextureMemByCategoryBound.clear() ;
	sTextureCurMemByCategoryBound.clear() ;
}

//static 
void LLImageGL::setHighlightTexture(S32 category) 
{
	const S32 dim = 128;
	sHighlightTexturep = new LLImageGL() ;
	LLPointer<LLImageRaw> image_raw = new LLImageRaw(dim,dim,3);
	U8* data = image_raw->getData();
	for (S32 i = 0; i<dim; i++)
	{
		for (S32 j = 0; j<dim; j++)
		{
			const S32 border = 2;
			if (i<border || j<border || i>=(dim-border) || j>=(dim-border))
			{
				*data++ = 0xff;
				*data++ = 0xff;
				*data++ = 0xff;
			}
			else
			{
				*data++ = 0xff;
				*data++ = 0xff;
				*data++ = 0x00;
			}
		}
	}
	sHighlightTexturep->createGLTexture(0, image_raw, 0, TRUE, category);
	image_raw = NULL;
}

//static
S32 LLImageGL::dataFormatBits(S32 dataformat)
{
	switch (dataformat)
	{
	  case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:	return 4;
	  case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:	return 8;
	  case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:	return 8;
	  case GL_LUMINANCE:						return 8;
	  case GL_ALPHA:							return 8;
	  case GL_COLOR_INDEX:						return 8;
	  case GL_LUMINANCE_ALPHA:					return 16;
	  case GL_RED:								return 8;
	  case GL_RG:								return 16;
	  case GL_RGB:								return 24;
	  case GL_RGB8:								return 24;
	  case GL_RGBA:								return 32;
	  case GL_BGRA:								return 32;		// Used for QuickTime media textures on the Mac
	  default:
		LL_ERRS() << "LLImageGL::Unknown format: " << dataformat << LL_ENDL;
		return 0;
	}
}

//static
S32 LLImageGL::dataFormatBytes(S32 dataformat, S32 width, S32 height)
{
	if (dataformat >= GL_COMPRESSED_RGB_S3TC_DXT1_EXT &&
		dataformat <= GL_COMPRESSED_RGBA_S3TC_DXT5_EXT)
	{
		if (width < 4) width = 4;
		if (height < 4) height = 4;
	}
	S32 bytes ((width*height*dataFormatBits(dataformat)+7)>>3);
	S32 aligned = (bytes+3)&~3;
	return aligned;
}

//static
S32 LLImageGL::dataFormatComponents(S32 dataformat)
{
	switch (dataformat)
	{
	  case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:	return 3;
	  case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:	return 4;
	  case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:	return 4;
	  case GL_LUMINANCE:						return 1;
	  case GL_ALPHA:							return 1;
	  case GL_COLOR_INDEX:						return 1;
	  case GL_LUMINANCE_ALPHA:					return 2;
	  case GL_RED:								return 1;
	  case GL_RG:								return 2;
	  case GL_RGB:								return 3;
	  case GL_RGBA:								return 4;
	  case GL_BGRA:								return 4;		// Used for QuickTime media textures on the Mac
	  default:
		LL_ERRS() << "LLImageGL::Unknown format: " << dataformat << LL_ENDL;
		return 0;
	}
}

//----------------------------------------------------------------------------

static LLFastTimer::DeclareTimer FTM_IMAGE_UPDATE_STATS("Image Stats");
// static
void LLImageGL::updateStats(F32 current_time)
{
	LLFastTimer t(FTM_IMAGE_UPDATE_STATS);
	sLastFrameTime = current_time;
	sBoundTextureMemoryInBytes = sCurBoundTextureMemory;
	sCurBoundTextureMemory = 0;

	if(gAuditTexture)
	{
		for(U32 i = 0 ; i < sTextureCurBoundCounter.size() ; i++)
		{
			sTextureBoundCounter[i] = sTextureCurBoundCounter[i] ;
			sTextureCurBoundCounter[i] = 0 ;
		}
		for(U32 i = 0 ; i < sTextureCurMemByCategoryBound.size() ; i++)
		{
			sTextureMemByCategoryBound[i] = sTextureCurMemByCategoryBound[i] ;
			sTextureCurMemByCategoryBound[i] = 0 ;
		}
	}
}

//static
S32 LLImageGL::updateBoundTexMem(const S32 mem, const S32 ncomponents, S32 category)
{
	if(gAuditTexture && ncomponents > 0 && category > -1)
	{
		sTextureCurBoundCounter[getTextureCounterIndex(mem / ncomponents)]++ ;
		sTextureCurMemByCategoryBound[category] += mem ;
	}
	
	LLImageGL::sCurBoundTextureMemory += mem ;
	return LLImageGL::sCurBoundTextureMemory;
}

//----------------------------------------------------------------------------

//static 
void LLImageGL::destroyGL(BOOL save_state)
{
	for (S32 stage = 0; stage < gGLManager.mNumTextureUnits; stage++)
	{
		gGL.getTexUnit(stage)->unbind(LLTexUnit::TT_TEXTURE);
	}
	
	sAllowReadBackRaw = true ;
	std::set<LLImageGL*> stored_images;
	for (std::set<LLImageGL*>::iterator iter = sImageList.begin();
		 iter != sImageList.end(); iter++)
	{
		LLImageGL* glimage = *iter;
		if (glimage->mTexName)
		{
			if (save_state && glimage->isGLTextureCreated() && glimage->mComponents)
			{
				glimage->mSaveData = new LLImageRaw;
				if(!glimage->readBackRaw(glimage->mCurrentDiscardLevel, glimage->mSaveData, false)) //necessary, keep it.
				{
					delete glimage;
				}
				else
				{
					glimage->mSaveDiscardLevel = glimage->mCurrentDiscardLevel;
					stored_images.insert(glimage);
					glimage->destroyGLTexture();
				}
			}
			
			stop_glerror();
		}
	}
	sImageList = stored_images;
	LL_INFOS() << "Storing " << stored_images.size() << " images..." << LL_ENDL;
	sAllowReadBackRaw = false ;
}

//static 
void LLImageGL::restoreGL()
{
	
	std::set<LLImageGL*> restored_images;
	for (std::set<LLImageGL*>::iterator iter = sImageList.begin();
		 iter != sImageList.end(); iter++)
	{
		LLImageGL* glimage = *iter;
		if(glimage->getTexName())
		{
			LL_ERRS() << "tex name is not 0." << LL_ENDL ;
		}
		if (glimage->mSaveData.notNull() && glimage->getComponents() &&
			glimage->mSaveData->getComponents() &&
			glimage->mSaveDiscardLevel >= 0 &&
			glimage->createGLTexture(glimage->mSaveDiscardLevel, glimage->mSaveData, 0, TRUE, glimage->getCategory()))
		{
				stop_glerror();
				restored_images.insert(glimage);
		}
		else
		{
			delete glimage;
		}
	}

	restored_images = restored_images;
	LL_INFOS() << "Restored " << restored_images.size() << " images" << LL_ENDL;
}

//static 
void LLImageGL::dirtyTexOptions()
{
	for (std::set<LLImageGL*>::iterator iter = sImageList.begin();
		 iter != sImageList.end(); iter++)
	{
		LLImageGL* glimage = *iter;
		glimage->mTexOptionsDirty = true;
		stop_glerror();
	}
	
}
//----------------------------------------------------------------------------

//for server side use only.
//static 
BOOL LLImageGL::create(LLPointer<LLImageGL>& dest, BOOL usemipmaps)
{
	dest = new LLImageGL(usemipmaps);
	return TRUE;
}

//for server side use only.
BOOL LLImageGL::create(LLPointer<LLImageGL>& dest, U32 width, U32 height, U8 components, BOOL usemipmaps)
{
	dest = new LLImageGL(width, height, components, usemipmaps);
	return TRUE;
}

//for server side use only.
BOOL LLImageGL::create(LLPointer<LLImageGL>& dest, const LLImageRaw* imageraw, BOOL usemipmaps)
{
	dest = new LLImageGL(imageraw, usemipmaps);
	return TRUE;
}

//----------------------------------------------------------------------------

LLImageGL::LLImageGL(BOOL usemipmaps)
	: mSaveData(0), mSaveDiscardLevel(-1)
{
	init(usemipmaps);
	setSize(0, 0, 0);
	sImageList.insert(this);
	sCount++;
}

LLImageGL::LLImageGL(U32 width, U32 height, U8 components, BOOL usemipmaps)
	: mSaveData(0), mSaveDiscardLevel(-1)
{
	llassert( components <= 4 );
	init(usemipmaps);
	setSize(width, height, components);
	sImageList.insert(this);
	sCount++;
}

LLImageGL::LLImageGL(const LLImageRaw* imageraw, BOOL usemipmaps)
	: mSaveData(0), mSaveDiscardLevel(-1)
{
	init(usemipmaps);
	setSize(0, 0, 0);
	sImageList.insert(this);
	sCount++;

	createGLTexture(0, imageraw); 
}

LLImageGL::~LLImageGL()
{
	LLImageGL::cleanup();
	sImageList.erase(this);
	delete [] mPickMask;
	mPickMask = NULL;
	sCount--;
}

const S8 INVALID_OFFSET = -99 ;

void LLImageGL::init(BOOL usemipmaps)
{
	// keep these members in the same order as declared in llimagehl.h
	// so that it is obvious by visual inspection if we forgot to
	// init a field.

	mTextureMemory = 0;
	mLastBindTime = 0.f;

	mPickMask = NULL;
	mPickMaskWidth = 0;
	mPickMaskHeight = 0;
	mUseMipMaps = usemipmaps;
	mHasExplicitFormat = FALSE;
	mAutoGenMips = FALSE;

	mIsMask = FALSE;
	mMaskRMSE = 1.f ;

	mNeedsAlphaAndPickMask = FALSE ;
	mAlphaStride = 0 ;
	mAlphaOffset = INVALID_OFFSET ;

	mGLTextureCreated = FALSE ;
	mTexName = 0;
	mWidth = 0;
	mHeight	= 0;
	mCurrentDiscardLevel = -1;	


	mAllowCompression = true;
	
	mTarget = GL_TEXTURE_2D;
	mBindTarget = LLTexUnit::TT_TEXTURE;
	mHasMipMaps = false;
	mMipLevels = -1;

	mIsResident = 0;

	mComponents = 0;
	mMaxDiscardLevel = MAX_DISCARD_LEVEL;

	mTexOptionsDirty = true;
	mAddressMode = LLTexUnit::TAM_WRAP;
	mFilterOption = LLTexUnit::TFO_ANISOTROPIC;
	
	mFormatInternal = -1;
	mFormatPrimary = (LLGLenum) 0;
	mFormatType = GL_UNSIGNED_BYTE;
	mFormatSwapBytes = FALSE;

#ifdef DEBUG_MISS
	mMissed	= FALSE;
#endif

	mCategory = -1;
}

void LLImageGL::cleanup()
{
	if (!gGLManager.mIsDisabled)
	{
		destroyGLTexture();
	}
	mSaveData = NULL; // deletes data
}

//----------------------------------------------------------------------------

//this function is used to check the size of a texture image.
//so dim should be a positive number
static bool check_power_of_two(S32 dim)
{
	if(dim < 0)
	{
		return false ;
	}
	if(!dim)//0 is a power-of-two number
	{
		return true ;
	}
	return !(dim & (dim - 1)) ;
}

//static
bool LLImageGL::checkSize(S32 width, S32 height)
{
	return check_power_of_two(width) && check_power_of_two(height);
}

void LLImageGL::setSize(S32 width, S32 height, S32 ncomponents, S32 discard_level)
{
	if (width != mWidth || height != mHeight || ncomponents != mComponents)
	{
		// Check if dimensions are a power of two!
		if (!checkSize(width,height))
		{
			LL_WARNS() << llformat("Texture has non power of two dimension: %dx%d",width,height) << "  Unless on Aurora-Sim, beware." << LL_ENDL;
		}
		
		if (mTexName)
		{
// 			LL_WARNS() << "Setting Size of LLImageGL with existing mTexName = " << mTexName << LL_ENDL;
			destroyGLTexture();
		}

		// pickmask validity depends on old image size, delete it
		delete [] mPickMask;
		mPickMask = NULL;
		mPickMaskWidth = mPickMaskHeight = 0;

		mWidth = width;
		mHeight = height;
		mComponents = ncomponents;
		if (ncomponents > 0)
		{
			mMaxDiscardLevel = 0;
			while (width > 1 && height > 1 && mMaxDiscardLevel < MAX_DISCARD_LEVEL)
			{
				mMaxDiscardLevel++;
				width >>= 1;
				height >>= 1;
			}

			if(discard_level > 0)
			{
				mMaxDiscardLevel = llmax(mMaxDiscardLevel, (S8)discard_level);
			}
		}
		else
		{
			mMaxDiscardLevel = MAX_DISCARD_LEVEL;
		}
	}
}

//----------------------------------------------------------------------------

// virtual
void LLImageGL::dump()
{
	LL_INFOS() << "mMaxDiscardLevel " << S32(mMaxDiscardLevel)
			<< " mLastBindTime " << mLastBindTime
			<< " mTarget " << S32(mTarget)
			<< " mBindTarget " << S32(mBindTarget)
			<< " mUseMipMaps " << S32(mUseMipMaps)
			<< " mHasMipMaps " << S32(mHasMipMaps)
			<< " mCurrentDiscardLevel " << S32(mCurrentDiscardLevel)
			<< " mFormatInternal " << S32(mFormatInternal)
			<< " mFormatPrimary " << S32(mFormatPrimary)
			<< " mFormatType " << S32(mFormatType)
			<< " mFormatSwapBytes " << S32(mFormatSwapBytes)
			<< " mHasExplicitFormat " << S32(mHasExplicitFormat)
#if DEBUG_MISS
			<< " mMissed " << mMissed
#endif
			<< LL_ENDL;

	LL_INFOS() << " mTextureMemory " << mTextureMemory
			<< " mTexNames " << mTexName
			<< " mIsResident " << S32(mIsResident)
			<< LL_ENDL;
}

//----------------------------------------------------------------------------
void LLImageGL::forceUpdateBindStats(void) const
{
	mLastBindTime = sLastFrameTime;
}

BOOL LLImageGL::updateBindStats(S32 tex_mem) const
{	
	if (mTexName != 0)
	{
#ifdef DEBUG_MISS
		mMissed = ! getIsResident(TRUE);
#endif
		sBindCount++;
		if (mLastBindTime != sLastFrameTime)
		{
			// we haven't accounted for this texture yet this frame
			sUniqueCount++;
			updateBoundTexMem(tex_mem, mComponents, mCategory);
			mLastBindTime = sLastFrameTime;

			return TRUE ;
		}
	}
	return FALSE ;
}

F32 LLImageGL::getTimePassedSinceLastBound()
{
	return sLastFrameTime - mLastBindTime ;
}

void LLImageGL::setExplicitFormat( LLGLint internal_format, LLGLenum primary_format, LLGLenum type_format, BOOL swap_bytes )
{
	// Note: must be called before createTexture()
	// Note: it's up to the caller to ensure that the format matches the number of components.
	mHasExplicitFormat = TRUE;
	mFormatInternal = internal_format;
	mFormatPrimary = primary_format;
	if(type_format == 0)
		mFormatType = GL_UNSIGNED_BYTE;
	else
		mFormatType = type_format;
	mFormatSwapBytes = swap_bytes;

	calcAlphaChannelOffsetAndStride() ;
}

//----------------------------------------------------------------------------

void LLImageGL::setImage(const LLImageRaw* imageraw)
{
	llassert((imageraw->getWidth() == getWidth(mCurrentDiscardLevel)) &&
			 (imageraw->getHeight() == getHeight(mCurrentDiscardLevel)) &&
			 (imageraw->getComponents() == getComponents()));
	const U8* rawdata = imageraw->getData();
	setImage(rawdata, FALSE);
}

static LLFastTimer::DeclareTimer FTM_SET_IMAGE("setImage");
void LLImageGL::setImage(const U8* data_in, BOOL data_hasmips)
{
	LLFastTimer t(FTM_SET_IMAGE);
	bool is_compressed = false;
	if (mFormatPrimary >= GL_COMPRESSED_RGBA_S3TC_DXT1_EXT && mFormatPrimary <= GL_COMPRESSED_RGBA_S3TC_DXT5_EXT)
	{
		is_compressed = true;
	}
	
	
	
	if (mUseMipMaps)
	{
		//set has mip maps to true before binding image so tex parameters get set properly
		gGL.getTexUnit(0)->unbind(mBindTarget);
		mHasMipMaps = true;
		mTexOptionsDirty = true;
		setFilteringOption(LLTexUnit::TFO_ANISOTROPIC);
	}
	else
	{
		mHasMipMaps = false;
	}
	
	llverify(gGL.getTexUnit(0)->bind(this));
	
	
	if (mUseMipMaps)
	{
		if (data_hasmips)
		{
			// NOTE: data_in points to largest image; smaller images
			// are stored BEFORE the largest image
			for (S32 d=mCurrentDiscardLevel; d<=mMaxDiscardLevel; d++)
			{
				
				S32 w = getWidth(d);
				S32 h = getHeight(d);
				S32 gl_level = d-mCurrentDiscardLevel;

				mMipLevels = llmax(mMipLevels, gl_level);

				if (d > mCurrentDiscardLevel)
				{
					data_in -= dataFormatBytes(mFormatPrimary, w, h); // see above comment
				}
				if (is_compressed)
				{
 					S32 tex_size = dataFormatBytes(mFormatPrimary, w, h);
					glCompressedTexImage2DARB(mTarget, gl_level, mFormatPrimary, w, h, 0, tex_size, (GLvoid *)data_in);
					stop_glerror();
				}
				else
				{
// 					LLFastTimer t2(FTM_TEMP4);

					if(mFormatSwapBytes)
					{
						glPixelStorei(GL_UNPACK_SWAP_BYTES, 1);
						stop_glerror();
					}
						
					LLImageGL::setManualImage(mTarget, gl_level, mFormatInternal, w, h, mFormatPrimary, GL_UNSIGNED_BYTE, (GLvoid*)data_in, mAllowCompression);
					if (gl_level == 0)
					{
						analyzeAlpha(data_in, w, h);
					}
					updatePickMask(w, h, data_in);

					if(mFormatSwapBytes)
					{
						glPixelStorei(GL_UNPACK_SWAP_BYTES, 0);
						stop_glerror();
					}
						
					stop_glerror();
				}
				stop_glerror();
			}			
		}
		else if (!is_compressed)
		{
			if (mAutoGenMips)
			{
				stop_glerror();
				{
// 					LLFastTimer t2(FTM_TEMP4);

					if(mFormatSwapBytes)
					{
						glPixelStorei(GL_UNPACK_SWAP_BYTES, 1);
						stop_glerror();
					}

					S32 w = getWidth(mCurrentDiscardLevel);
					S32 h = getHeight(mCurrentDiscardLevel);

					mMipLevels = wpo2(llmax(w, h));

 					//use legacy mipmap generation mode (note: making this condional can cause rendering issues)
					// -- but making it not conditional triggers deprecation warnings when core profile is enabled
					//		(some rendering issues while core profile is enabled are acceptable at this point in time)
					if (!LLRender::sGLCoreProfile)
					{
						glTexParameteri(mTarget, GL_GENERATE_MIPMAP, GL_TRUE);
					}
					
					LLImageGL::setManualImage(mTarget, 0, mFormatInternal,
								 w, h, 
								 mFormatPrimary, mFormatType,
								 data_in, mAllowCompression);
					analyzeAlpha(data_in, w, h);
					stop_glerror();

					updatePickMask(w, h, data_in);

					if(mFormatSwapBytes)
					{
						glPixelStorei(GL_UNPACK_SWAP_BYTES, 0);
						stop_glerror();
					}

					if (LLRender::sGLCoreProfile)
					{
						glGenerateMipmap(mTarget);
					}	
					stop_glerror();
				}
			}
			else
			{
				// Create mips by hand
				// ~4x faster than gluBuild2DMipmaps
				S32 width = getWidth(mCurrentDiscardLevel);
				S32 height = getHeight(mCurrentDiscardLevel);
				S32 nummips = mMaxDiscardLevel - mCurrentDiscardLevel + 1;
				S32 w = width, h = height;
				const U8* prev_mip_data = 0;
				const U8* cur_mip_data = 0;
#ifdef SHOW_ASSERT
				S32 cur_mip_size = 0;
#endif
				
				mMipLevels = nummips;

				for (int m=0; m<nummips; m++)
				{
					if (m==0)
					{
						cur_mip_data = data_in;
#ifdef SHOW_ASSERT
						cur_mip_size = width * height * mComponents; 
#endif
					}
					else
					{
						S32 bytes = w * h * mComponents;
#ifdef SHOW_ASSERT
						llassert(prev_mip_data);
						llassert(cur_mip_size == bytes*4);
#endif
						U8* new_data = new U8[bytes];
						llassert_always(new_data);
						LLImageBase::generateMip(prev_mip_data, new_data, w, h, mComponents);
						cur_mip_data = new_data;
#ifdef SHOW_ASSERT
						cur_mip_size = bytes; 
#endif
					}
					llassert(w > 0 && h > 0 && cur_mip_data);
					{
// 						LLFastTimer t1(FTM_TEMP4);
						if(mFormatSwapBytes)
						{
							glPixelStorei(GL_UNPACK_SWAP_BYTES, 1);
							stop_glerror();
						}

						LLImageGL::setManualImage(mTarget, m, mFormatInternal, w, h, mFormatPrimary, mFormatType, cur_mip_data, mAllowCompression);
						if (m == 0)
						{
							analyzeAlpha(data_in, w, h);
						}
						stop_glerror();
						if (m == 0)
						{
							updatePickMask(w, h, cur_mip_data);
						}

						if(mFormatSwapBytes)
						{
							glPixelStorei(GL_UNPACK_SWAP_BYTES, 0);
							stop_glerror();
						}
					}
					if (prev_mip_data && prev_mip_data != data_in)
					{
						delete[] prev_mip_data;
					}
					prev_mip_data = cur_mip_data;
					w >>= 1;
					h >>= 1;
				}
				if (prev_mip_data && prev_mip_data != data_in)
				{
					delete[] prev_mip_data;
					prev_mip_data = NULL;
				}
			}
		}
		else
		{
			LL_ERRS() << "Compressed Image has mipmaps but data does not (can not auto generate compressed mips)" << LL_ENDL;
		}
	}
	else
	{
		mMipLevels = 0;
		S32 w = getWidth();
		S32 h = getHeight();
		if (is_compressed)
		{
			S32 tex_size = dataFormatBytes(mFormatPrimary, w, h);
			glCompressedTexImage2DARB(mTarget, 0, mFormatPrimary, w, h, 0, tex_size, (GLvoid *)data_in);
			stop_glerror();
		}
		else
		{
			if(mFormatSwapBytes)
			{
				glPixelStorei(GL_UNPACK_SWAP_BYTES, 1);
				stop_glerror();
			}

			LLImageGL::setManualImage(mTarget, 0, mFormatInternal, w, h,
						 mFormatPrimary, mFormatType, (GLvoid *)data_in, mAllowCompression);
			analyzeAlpha(data_in, w, h);
			
			updatePickMask(w, h, data_in);

			stop_glerror();

			if(mFormatSwapBytes)
			{
				glPixelStorei(GL_UNPACK_SWAP_BYTES, 0);
				stop_glerror();
			}

		}
	}
	stop_glerror();
	mGLTextureCreated = true;
}

BOOL LLImageGL::setSubImage(const U8* datap, S32 data_width, S32 data_height, S32 x_pos, S32 y_pos, S32 width, S32 height, BOOL force_fast_update)
{
	if (!width || !height)
	{
		return TRUE;
	}
	if (mTexName == 0)
	{
		// *TODO: Re-enable warning?  Ran into thread locking issues? DK 2011-02-18
		//LL_WARNS() << "Setting subimage on image without GL texture" << LL_ENDL;
		return FALSE;
	}
	if (datap == NULL)
	{
		// *TODO: Re-enable warning?  Ran into thread locking issues? DK 2011-02-18
		//LL_WARNS() << "Setting subimage on image with NULL datap" << LL_ENDL;
		return FALSE;
	}
	
	// HACK: allow the caller to explicitly force the fast path (i.e. using glTexSubImage2D here instead of calling setImage) even when updating the full texture.
	if (!force_fast_update && x_pos == 0 && y_pos == 0 && width == getWidth() && height == getHeight() && data_width == width && data_height == height)
	{
		setImage(datap, FALSE);
	}
	else
	{
		if (mUseMipMaps)
		{
			dump();
			LL_ERRS() << "setSubImage called with mipmapped image (not supported)" << LL_ENDL;
		}
		llassert_always(mCurrentDiscardLevel == 0);
		llassert_always(x_pos >= 0 && y_pos >= 0);
		
		if (((x_pos + width) > getWidth()) || 
			(y_pos + height) > getHeight())
		{
			dump();
			LL_ERRS() << "Subimage not wholly in target image!" 
				   << " x_pos " << x_pos
				   << " y_pos " << y_pos
				   << " width " << width
				   << " height " << height
				   << " getWidth() " << getWidth()
				   << " getHeight() " << getHeight()
				   << LL_ENDL;
		}

		if ((x_pos + width) > data_width || 
			(y_pos + height) > data_height)
		{
			dump();
			LL_ERRS() << "Subimage not wholly in source image!" 
				   << " x_pos " << x_pos
				   << " y_pos " << y_pos
				   << " width " << width
				   << " height " << height
				   << " source_width " << data_width
				   << " source_height " << data_height
				   << LL_ENDL;
		}


		glPixelStorei(GL_UNPACK_ROW_LENGTH, data_width);
		stop_glerror();

		if(mFormatSwapBytes)
		{
			glPixelStorei(GL_UNPACK_SWAP_BYTES, 1);
			stop_glerror();
		}

		datap += (y_pos * data_width + x_pos) * getComponents();
		// Update the GL texture
		BOOL res = gGL.getTexUnit(0)->bindManual(mBindTarget, mTexName);
		if (!res) LL_ERRS() << "LLImageGL::setSubImage(): bindTexture failed" << LL_ENDL;
		stop_glerror();

		glTexSubImage2D(mTarget, 0, x_pos, y_pos, 
						width, height, mFormatPrimary, mFormatType, datap);
		gGL.getTexUnit(0)->disable();
		stop_glerror();

		if(mFormatSwapBytes)
		{
			glPixelStorei(GL_UNPACK_SWAP_BYTES, 0);
			stop_glerror();
		}

		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		stop_glerror();
		mGLTextureCreated = true;
	}
	return TRUE;
}

BOOL LLImageGL::setSubImage(const LLImageRaw* imageraw, S32 x_pos, S32 y_pos, S32 width, S32 height, BOOL force_fast_update)
{
	return setSubImage(imageraw->getData(), imageraw->getWidth(), imageraw->getHeight(), x_pos, y_pos, width, height, force_fast_update);
}

// Copy sub image from frame buffer
BOOL LLImageGL::setSubImageFromFrameBuffer(S32 fb_x, S32 fb_y, S32 x_pos, S32 y_pos, S32 width, S32 height)
{
	if (gGL.getTexUnit(0)->bind(this, false, true))
	{
		//checkTexSize() ;
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, fb_x, fb_y, x_pos, y_pos, width, height);
		mGLTextureCreated = true;
		stop_glerror();
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

// static
static LLFastTimer::DeclareTimer FTM_GENERATE_TEXTURES("generate textures");
void LLImageGL::generateTextures(S32 numTextures, U32 *textures)
{
	LLFastTimer t(FTM_GENERATE_TEXTURES);
	glGenTextures(numTextures, textures);
}

// static
void LLImageGL::deleteTextures(S32 numTextures, U32 *textures)
{
	if (gGLManager.mInited)
	{
		glDeleteTextures(numTextures, textures);
	}
}

//#include "crnlib.h"
struct DDS_PIXELFORMAT {
	U32 dwSize;
	U32 dwFlags;
	U32 dwFourCC;
	U32 dwRGBBitCount;
	U32 dwRBitMask;
	U32 dwGBitMask;
	U32 dwBBitMask;
	U32 dwABitMask;
};

typedef struct {
	U32           dwSize;
	U32           dwFlags;
	U32           dwHeight;
	U32           dwWidth;
	U32           dwPitchOrLinearSize;
	U32           dwDepth;
	U32           dwMipMapCount;
	U32           dwReserved1[11];
	DDS_PIXELFORMAT ddspf;
	U32           dwCaps;
	U32           dwCaps2;
	U32           dwCaps3;
	U32           dwCaps4;
	U32           dwReserved2;
} DDS_HEADER;

typedef enum DXGI_FORMAT {
	DXGI_FORMAT_UNKNOWN = 0,
	DXGI_FORMAT_R32G32B32A32_TYPELESS = 1,
	DXGI_FORMAT_R32G32B32A32_FLOAT = 2,
	DXGI_FORMAT_R32G32B32A32_UINT = 3,
	DXGI_FORMAT_R32G32B32A32_SINT = 4,
	DXGI_FORMAT_R32G32B32_TYPELESS = 5,
	DXGI_FORMAT_R32G32B32_FLOAT = 6,
	DXGI_FORMAT_R32G32B32_UINT = 7,
	DXGI_FORMAT_R32G32B32_SINT = 8,
	DXGI_FORMAT_R16G16B16A16_TYPELESS = 9,
	DXGI_FORMAT_R16G16B16A16_FLOAT = 10,
	DXGI_FORMAT_R16G16B16A16_UNORM = 11,
	DXGI_FORMAT_R16G16B16A16_UINT = 12,
	DXGI_FORMAT_R16G16B16A16_SNORM = 13,
	DXGI_FORMAT_R16G16B16A16_SINT = 14,
	DXGI_FORMAT_R32G32_TYPELESS = 15,
	DXGI_FORMAT_R32G32_FLOAT = 16,
	DXGI_FORMAT_R32G32_UINT = 17,
	DXGI_FORMAT_R32G32_SINT = 18,
	DXGI_FORMAT_R32G8X24_TYPELESS = 19,
	DXGI_FORMAT_D32_FLOAT_S8X24_UINT = 20,
	DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS = 21,
	DXGI_FORMAT_X32_TYPELESS_G8X24_UINT = 22,
	DXGI_FORMAT_R10G10B10A2_TYPELESS = 23,
	DXGI_FORMAT_R10G10B10A2_UNORM = 24,
	DXGI_FORMAT_R10G10B10A2_UINT = 25,
	DXGI_FORMAT_R11G11B10_FLOAT = 26,
	DXGI_FORMAT_R8G8B8A8_TYPELESS = 27,
	DXGI_FORMAT_R8G8B8A8_UNORM = 28,
	DXGI_FORMAT_R8G8B8A8_UNORM_SRGB = 29,
	DXGI_FORMAT_R8G8B8A8_UINT = 30,
	DXGI_FORMAT_R8G8B8A8_SNORM = 31,
	DXGI_FORMAT_R8G8B8A8_SINT = 32,
	DXGI_FORMAT_R16G16_TYPELESS = 33,
	DXGI_FORMAT_R16G16_FLOAT = 34,
	DXGI_FORMAT_R16G16_UNORM = 35,
	DXGI_FORMAT_R16G16_UINT = 36,
	DXGI_FORMAT_R16G16_SNORM = 37,
	DXGI_FORMAT_R16G16_SINT = 38,
	DXGI_FORMAT_R32_TYPELESS = 39,
	DXGI_FORMAT_D32_FLOAT = 40,
	DXGI_FORMAT_R32_FLOAT = 41,
	DXGI_FORMAT_R32_UINT = 42,
	DXGI_FORMAT_R32_SINT = 43,
	DXGI_FORMAT_R24G8_TYPELESS = 44,
	DXGI_FORMAT_D24_UNORM_S8_UINT = 45,
	DXGI_FORMAT_R24_UNORM_X8_TYPELESS = 46,
	DXGI_FORMAT_X24_TYPELESS_G8_UINT = 47,
	DXGI_FORMAT_R8G8_TYPELESS = 48,
	DXGI_FORMAT_R8G8_UNORM = 49,
	DXGI_FORMAT_R8G8_UINT = 50,
	DXGI_FORMAT_R8G8_SNORM = 51,
	DXGI_FORMAT_R8G8_SINT = 52,
	DXGI_FORMAT_R16_TYPELESS = 53,
	DXGI_FORMAT_R16_FLOAT = 54,
	DXGI_FORMAT_D16_UNORM = 55,
	DXGI_FORMAT_R16_UNORM = 56,
	DXGI_FORMAT_R16_UINT = 57,
	DXGI_FORMAT_R16_SNORM = 58,
	DXGI_FORMAT_R16_SINT = 59,
	DXGI_FORMAT_R8_TYPELESS = 60,
	DXGI_FORMAT_R8_UNORM = 61,
	DXGI_FORMAT_R8_UINT = 62,
	DXGI_FORMAT_R8_SNORM = 63,
	DXGI_FORMAT_R8_SINT = 64,
	DXGI_FORMAT_A8_UNORM = 65,
	DXGI_FORMAT_R1_UNORM = 66,
	DXGI_FORMAT_R9G9B9E5_SHAREDEXP = 67,
	DXGI_FORMAT_R8G8_B8G8_UNORM = 68,
	DXGI_FORMAT_G8R8_G8B8_UNORM = 69,
	DXGI_FORMAT_BC1_TYPELESS = 70,
	DXGI_FORMAT_BC1_UNORM = 71,
	DXGI_FORMAT_BC1_UNORM_SRGB = 72,
	DXGI_FORMAT_BC2_TYPELESS = 73,
	DXGI_FORMAT_BC2_UNORM = 74,
	DXGI_FORMAT_BC2_UNORM_SRGB = 75,
	DXGI_FORMAT_BC3_TYPELESS = 76,
	DXGI_FORMAT_BC3_UNORM = 77,
	DXGI_FORMAT_BC3_UNORM_SRGB = 78,
	DXGI_FORMAT_BC4_TYPELESS = 79,
	DXGI_FORMAT_BC4_UNORM = 80,
	DXGI_FORMAT_BC4_SNORM = 81,
	DXGI_FORMAT_BC5_TYPELESS = 82,
	DXGI_FORMAT_BC5_UNORM = 83,
	DXGI_FORMAT_BC5_SNORM = 84,
	DXGI_FORMAT_B5G6R5_UNORM = 85,
	DXGI_FORMAT_B5G5R5A1_UNORM = 86,
	DXGI_FORMAT_B8G8R8A8_UNORM = 87,
	DXGI_FORMAT_B8G8R8X8_UNORM = 88,
	DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM = 89,
	DXGI_FORMAT_B8G8R8A8_TYPELESS = 90,
	DXGI_FORMAT_B8G8R8A8_UNORM_SRGB = 91,
	DXGI_FORMAT_B8G8R8X8_TYPELESS = 92,
	DXGI_FORMAT_B8G8R8X8_UNORM_SRGB = 93,
	DXGI_FORMAT_BC6H_TYPELESS = 94,
	DXGI_FORMAT_BC6H_UF16 = 95,
	DXGI_FORMAT_BC6H_SF16 = 96,
	DXGI_FORMAT_BC7_TYPELESS = 97,
	DXGI_FORMAT_BC7_UNORM = 98,
	DXGI_FORMAT_BC7_UNORM_SRGB = 99,
	DXGI_FORMAT_AYUV = 100,
	DXGI_FORMAT_Y410 = 101,
	DXGI_FORMAT_Y416 = 102,
	DXGI_FORMAT_NV12 = 103,
	DXGI_FORMAT_P010 = 104,
	DXGI_FORMAT_P016 = 105,
	DXGI_FORMAT_420_OPAQUE = 106,
	DXGI_FORMAT_YUY2 = 107,
	DXGI_FORMAT_Y210 = 108,
	DXGI_FORMAT_Y216 = 109,
	DXGI_FORMAT_NV11 = 110,
	DXGI_FORMAT_AI44 = 111,
	DXGI_FORMAT_IA44 = 112,
	DXGI_FORMAT_P8 = 113,
	DXGI_FORMAT_A8P8 = 114,
	DXGI_FORMAT_B4G4R4A4_UNORM = 115,
	DXGI_FORMAT_P208 = 130,
	DXGI_FORMAT_V208 = 131,
	DXGI_FORMAT_V408 = 132,
	DXGI_FORMAT_ASTC_4X4_UNORM = 134,
	DXGI_FORMAT_ASTC_4X4_UNORM_SRGB = 135,
	DXGI_FORMAT_ASTC_5X4_TYPELESS = 137,
	DXGI_FORMAT_ASTC_5X4_UNORM = 138,
	DXGI_FORMAT_ASTC_5X4_UNORM_SRGB = 139,
	DXGI_FORMAT_ASTC_5X5_TYPELESS = 141,
	DXGI_FORMAT_ASTC_5X5_UNORM = 142,
	DXGI_FORMAT_ASTC_5X5_UNORM_SRGB = 143,
	DXGI_FORMAT_ASTC_6X5_TYPELESS = 145,
	DXGI_FORMAT_ASTC_6X5_UNORM = 146,
	DXGI_FORMAT_ASTC_6X5_UNORM_SRGB = 147,
	DXGI_FORMAT_ASTC_6X6_TYPELESS = 149,
	DXGI_FORMAT_ASTC_6X6_UNORM = 150,
	DXGI_FORMAT_ASTC_6X6_UNORM_SRGB = 151,
	DXGI_FORMAT_ASTC_8X5_TYPELESS = 153,
	DXGI_FORMAT_ASTC_8X5_UNORM = 154,
	DXGI_FORMAT_ASTC_8X5_UNORM_SRGB = 155,
	DXGI_FORMAT_ASTC_8X6_TYPELESS = 157,
	DXGI_FORMAT_ASTC_8X6_UNORM = 158,
	DXGI_FORMAT_ASTC_8X6_UNORM_SRGB = 159,
	DXGI_FORMAT_ASTC_8X8_TYPELESS = 161,
	DXGI_FORMAT_ASTC_8X8_UNORM = 162,
	DXGI_FORMAT_ASTC_8X8_UNORM_SRGB = 163,
	DXGI_FORMAT_ASTC_10X5_TYPELESS = 165,
	DXGI_FORMAT_ASTC_10X5_UNORM = 166,
	DXGI_FORMAT_ASTC_10X5_UNORM_SRGB = 167,
	DXGI_FORMAT_ASTC_10X6_TYPELESS = 169,
	DXGI_FORMAT_ASTC_10X6_UNORM = 170,
	DXGI_FORMAT_ASTC_10X6_UNORM_SRGB = 171,
	DXGI_FORMAT_ASTC_10X8_TYPELESS = 173,
	DXGI_FORMAT_ASTC_10X8_UNORM = 174,
	DXGI_FORMAT_ASTC_10X8_UNORM_SRGB = 175,
	DXGI_FORMAT_ASTC_10X10_TYPELESS = 177,
	DXGI_FORMAT_ASTC_10X10_UNORM = 178,
	DXGI_FORMAT_ASTC_10X10_UNORM_SRGB = 179,
	DXGI_FORMAT_ASTC_12X10_TYPELESS = 181,
	DXGI_FORMAT_ASTC_12X10_UNORM = 182,
	DXGI_FORMAT_ASTC_12X10_UNORM_SRGB = 183,
	DXGI_FORMAT_ASTC_12X12_TYPELESS = 185,
	DXGI_FORMAT_ASTC_12X12_UNORM = 186,
	DXGI_FORMAT_ASTC_12X12_UNORM_SRGB = 187,
	DXGI_FORMAT_FORCE_UINT = 0xffffffff
} DXGI_FORMAT;

typedef enum D3D10_RESOURCE_DIMENSION {
	D3D10_RESOURCE_DIMENSION_UNKNOWN = 0,
	D3D10_RESOURCE_DIMENSION_BUFFER = 1,
	D3D10_RESOURCE_DIMENSION_TEXTURE1D = 2,
	D3D10_RESOURCE_DIMENSION_TEXTURE2D = 3,
	D3D10_RESOURCE_DIMENSION_TEXTURE3D = 4
} D3D10_RESOURCE_DIMENSION;

typedef struct {
	DXGI_FORMAT              dxgiFormat;
	D3D10_RESOURCE_DIMENSION resourceDimension;
	U32                      miscFlag;
	U32                      arraySize;
	U32                      miscFlags2;
} DDS_HEADER_DXT10;

// static
static LLFastTimer::DeclareTimer FTM_SET_MANUAL_IMAGE("setManualImage");
void LLImageGL::setManualImage(U32 target, S32 miplevel, S32 intformat, S32 width, S32 height, U32 pixformat, U32 pixtype, const void *pixels, bool allow_compression)
{
	LLFastTimer t(FTM_SET_MANUAL_IMAGE);
	std::vector<U32> scratch;
	if (LLRender::sGLCoreProfile)
	{
#ifdef GL_ARB_texture_swizzle
		if(gGLManager.mHasTextureSwizzle)
		{
			if (pixformat == GL_ALPHA)
			{ //GL_ALPHA is deprecated, convert to RGBA
				const GLint mask[] = {GL_ZERO, GL_ZERO, GL_ZERO, GL_RED};
				glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, mask);
				pixformat = GL_RED;
				intformat = GL_R8;
			}

			if (pixformat == GL_LUMINANCE)
			{ //GL_LUMINANCE is deprecated, convert to GL_RGBA
				const GLint mask[] = {GL_RED, GL_RED, GL_RED, GL_ONE};
				glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, mask);
				pixformat = GL_RED;
				intformat = GL_R8;
			}

			if (pixformat == GL_LUMINANCE_ALPHA)
			{ //GL_LUMINANCE_ALPHA is deprecated, convert to RGBA
				const GLint mask[] = {GL_RED, GL_RED, GL_RED, GL_GREEN};
				glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, mask);
				pixformat = GL_RG;
				intformat = GL_RG8;
			}
		}
		else
#endif
		{
			if (pixformat == GL_ALPHA && pixtype == GL_UNSIGNED_BYTE)
			{ //GL_ALPHA is deprecated, convert to RGBA
				scratch.resize(width*height);

				U32 pixel_count = (U32) (width*height);
				for (U32 i = 0; i < pixel_count; i++)
				{
					U8* pix = (U8*) &scratch[i];
					pix[0] = pix[1] = pix[2] = 0;
					pix[3] = ((U8*) pixels)[i];
				}

				pixels = &scratch[0];

				pixformat = GL_RGBA;
				intformat = GL_RGBA8;
			}

			if (pixformat == GL_LUMINANCE_ALPHA && pixtype == GL_UNSIGNED_BYTE)
			{ //GL_LUMINANCE_ALPHA is deprecated, convert to RGBA
				scratch.resize(width*height);

				U32 pixel_count = (U32) (width*height);
				for (U32 i = 0; i < pixel_count; i++)
				{
					U8 lum = ((U8*) pixels)[i*2+0];
					U8 alpha = ((U8*) pixels)[i*2+1];

					U8* pix = (U8*) &scratch[i];
					pix[0] = pix[1] = pix[2] = lum;
					pix[3] = alpha;
				}

				pixels = &scratch[0];

				pixformat = GL_RGBA;
				intformat = GL_RGBA8;
			}

			if (pixformat == GL_LUMINANCE && pixtype == GL_UNSIGNED_BYTE)
			{ //GL_LUMINANCE_ALPHA is deprecated, convert to RGB
				scratch.resize(width*height);

				U32 pixel_count = (U32) (width*height);
				for (U32 i = 0; i < pixel_count; i++)
				{
					U8 lum = ((U8*) pixels)[i];

					U8* pix = (U8*) &scratch[i];
					pix[0] = pix[1] = pix[2] = lum;
					pix[3] = 255;
				}

				pixels = &scratch[0];

				pixformat = GL_RGBA;
				intformat = GL_RGB8;
			}
		}
	}
	if (LLImageGL::sCompressTextures && allow_compression)
	{
		switch (intformat)
		{
			case GL_RED: 
			case GL_R8:
				intformat = GL_COMPRESSED_RED; 
				break;
			case GL_RG: 
			case GL_RG8:
				intformat = GL_COMPRESSED_RG; 
				break;
			case GL_RGB: 
			case GL_RGB8:
			{
				/*std::vector<U32> tex;
				tex.resize(height*width);
				for (U32 i = 0; i < tex.size(); ++i)
				{
					((U8*)&tex[i])[0] = ((U8*)pixels)[i * 3];
					((U8*)&tex[i])[1] = ((U8*)pixels)[i * 3 + 1];
					((U8*)&tex[i])[2] = ((U8*)pixels)[i * 3 + 2];
					((U8*)&tex[i])[3] = 255;
				}
				crn_comp_params comp_params;
				comp_params.m_width = width;
				comp_params.m_height = height;
				comp_params.set_flag(cCRNCompFlagPerceptual, true);
				comp_params.set_flag(cCRNCompFlagHierarchical, false);
				comp_params.m_file_type = cCRNFileTypeDDS;
				comp_params.m_format = cCRNFmtDXT5;
				comp_params.m_pImages[0][0] = &tex[0];
				comp_params.m_quality_level = cCRNDXTQualityUber;
				SYSTEM_INFO g_system_info;
				GetSystemInfo(&g_system_info);
				comp_params.m_num_helper_threads = std::max<S32>(0, (S32)g_system_info.dwNumberOfProcessors - 1);
				crn_mipmap_params mip_params;
				mip_params.m_gamma_filtering = true;
				mip_params.m_mode = cCRNMipModeGenerateMips;

				crn_uint32 output_file_size;
				void *compressed_data = crn_compress(comp_params, mip_params, output_file_size);
				if (compressed_data)
				{
					glTexParameteri(target, GL_GENERATE_MIPMAP, GL_FALSE);

					U32 pos = sizeof(U32);
					const DDS_HEADER& header = *(DDS_HEADER*)(((U8*)compressed_data) + pos);
					pos += sizeof(DDS_HEADER);
					if (header.ddspf.dwFlags & 0x4 && header.ddspf.dwFourCC == '01XD')
					{
						pos += sizeof(DDS_HEADER_DXT10);
					}

					U32 num_mips = (header.dwFlags & 0x20000) ? header.dwMipMapCount : 1;

					U32 x = width;
					U32 y = height;
					for (U32 i = 0; i < num_mips; ++i)
					{
						size_t size = llmax(4u, x) / 4 * llmax(4u, y) / 4 * 16;
						glCompressedTexImage2DARB(GL_TEXTURE_2D, i, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, x, y, 0, size, (const void*)(((U8*)compressed_data) + pos));
						x = (x + 1) >> 1;
						y = (y + 1) >> 1;
						pos += size;
					}
					crn_free_block(compressed_data);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, num_mips - 1);
					return;
				}*/
			}

				break;
			case GL_RGBA:
			case GL_RGBA8:
				//intformat = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;
				break;
			case GL_LUMINANCE:
			case GL_LUMINANCE8:
				intformat = GL_COMPRESSED_LUMINANCE;
				break;
			case GL_LUMINANCE_ALPHA:
			case GL_LUMINANCE8_ALPHA8:
				intformat = GL_COMPRESSED_LUMINANCE_ALPHA;
				break;
			case GL_ALPHA:
			case GL_ALPHA8:
				intformat = GL_COMPRESSED_ALPHA;
				break;
			default:
				LL_WARNS() << "Could not compress format: " << std::hex << intformat << std::dec << LL_ENDL;
				break;
		}
	}

	stop_glerror();
	glTexImage2D(target, miplevel, intformat, width, height, 0, pixformat, pixtype, pixels);
	stop_glerror();
}

//create an empty GL texture: just create a texture name
//the texture is assiciate with some image by calling glTexImage outside LLImageGL
static LLFastTimer::DeclareTimer FTM_CREATE_GL_TEXTURE1("createGLTexture()");
BOOL LLImageGL::createGLTexture()
{
	LLFastTimer t(FTM_CREATE_GL_TEXTURE1);
	if (gGLManager.mIsDisabled)
	{
		LL_WARNS() << "Trying to create a texture while GL is disabled!" << LL_ENDL;
		return FALSE;
	}
	
	mGLTextureCreated = false ; //do not save this texture when gl is destroyed.

	llassert(gGLManager.mInited);
	stop_glerror();

	if(mTexName)
	{
		LLImageGL::deleteTextures(1, (reinterpret_cast<GLuint*>(&mTexName))) ;
	}
	

	LLImageGL::generateTextures(1, &mTexName);
	stop_glerror();
	if (!mTexName)
	{
		LL_ERRS() << "LLImageGL::createGLTexture failed to make an empty texture" << LL_ENDL;
	}

	return TRUE ;
}

static LLFastTimer::DeclareTimer FTM_CREATE_GL_TEXTURE2("createGLTexture(raw)");
BOOL LLImageGL::createGLTexture(S32 discard_level, const LLImageRaw* imageraw, S32 usename/*=0*/, BOOL to_create, S32 category)
{
	LLFastTimer t(FTM_CREATE_GL_TEXTURE2);
	if (gGLManager.mIsDisabled)
	{
		LL_WARNS() << "Trying to create a texture while GL is disabled!" << LL_ENDL;
		return FALSE;
	}

	mGLTextureCreated = false ;
	llassert(gGLManager.mInited);
	stop_glerror();

	if (discard_level < 0)
	{
		llassert(mCurrentDiscardLevel >= 0);
		discard_level = mCurrentDiscardLevel;
	}
	
	// Actual image width/height = raw image width/height * 2^discard_level
	S32 raw_w = imageraw->getWidth() ;
	S32 raw_h = imageraw->getHeight() ;
	S32 w = raw_w << discard_level;
	S32 h = raw_h << discard_level;

	// setSize may call destroyGLTexture if the size does not match
	setSize(w, h, imageraw->getComponents(), discard_level);

	if( !mHasExplicitFormat )
	{
		switch (mComponents)
		{
			case 1:
			// Use luminance alpha (for fonts)
			mFormatInternal = GL_LUMINANCE8;
			mFormatPrimary = GL_LUMINANCE;
			mFormatType = GL_UNSIGNED_BYTE;
			break;
			case 2:
			// Use luminance alpha (for fonts)
			mFormatInternal = GL_LUMINANCE8_ALPHA8;
			mFormatPrimary = GL_LUMINANCE_ALPHA;
			mFormatType = GL_UNSIGNED_BYTE;
			break;
			case 3:
			mFormatInternal = GL_RGB8;
			mFormatPrimary = GL_RGB;
			mFormatType = GL_UNSIGNED_BYTE;
			break;
			case 4:
			mFormatInternal = GL_RGBA8;
			mFormatPrimary = GL_RGBA;
			mFormatType = GL_UNSIGNED_BYTE;
			break;
			default:
			LL_DEBUGS("Openjpeg") << "Bad number of components for texture: " << (U32)getComponents() << LL_ENDL;
			to_create = false;
			break;
		}

		calcAlphaChannelOffsetAndStride() ;
	}

	if(!to_create) //not create a gl texture
	{
		destroyGLTexture();
		mCurrentDiscardLevel = discard_level;	
		mLastBindTime = sLastFrameTime;
		return TRUE ;
	}

	setCategory(category);
 	const U8* rawdata = imageraw->getData();
	return createGLTexture(discard_level, rawdata, FALSE, usename);
}

static LLFastTimer::DeclareTimer FTM_CREATE_GL_TEXTURE3("createGLTexture3(data)");
BOOL LLImageGL::createGLTexture(S32 discard_level, const U8* data_in, BOOL data_hasmips, S32 usename)
{
	LLFastTimer t(FTM_CREATE_GL_TEXTURE3);
	llassert(data_in);
	stop_glerror();

	if (discard_level < 0)
	{
		llassert(mCurrentDiscardLevel >= 0);
		discard_level = mCurrentDiscardLevel;
	}
	discard_level = llclamp(discard_level, 0, (S32)mMaxDiscardLevel);

	if (mTexName != 0 && discard_level == mCurrentDiscardLevel)
	{
		// This will only be true if the size has not changed
		setImage(data_in, data_hasmips);
		return TRUE;
	}
	
	U32 old_name = mTexName;
// 	S32 old_discard = mCurrentDiscardLevel;
	
	if (usename != 0)
	{
		mTexName = usename;
	}
	else
	{
		LLImageGL::generateTextures(1, &mTexName);
		stop_glerror();
		{
			llverify(gGL.getTexUnit(0)->bind(this));
			stop_glerror();
			glTexParameteri(LLTexUnit::getInternalType(mBindTarget), GL_TEXTURE_BASE_LEVEL, 0);
			stop_glerror();
			glTexParameteri(LLTexUnit::getInternalType(mBindTarget), GL_TEXTURE_MAX_LEVEL,  mMaxDiscardLevel-discard_level);
			stop_glerror();
		}
	}
	if (!mTexName)
	{
		LL_ERRS() << "LLImageGL::createGLTexture failed to make texture" << LL_ENDL;
	}

	if (mUseMipMaps)
	{
		mAutoGenMips = gGLManager.mHasMipMapGeneration;
#if LL_DARWIN
		// On the Mac GF2 and GF4MX drivers, auto mipmap generation doesn't work right with alpha-only textures.
		if(gGLManager.mIsGF2or4MX && (mFormatInternal == GL_ALPHA8) && (mFormatPrimary == GL_ALPHA))
		{
			mAutoGenMips = FALSE;
		}
#endif
	}

	mCurrentDiscardLevel = discard_level;	

	setImage(data_in, data_hasmips);

	// Set texture options to our defaults.
	gGL.getTexUnit(0)->setHasMipMaps(mHasMipMaps);
	gGL.getTexUnit(0)->setTextureAddressMode(mAddressMode);
	gGL.getTexUnit(0)->setTextureFilteringOption(mFilterOption);

	// things will break if we don't unbind after creation
	gGL.getTexUnit(0)->unbind(mBindTarget);
	stop_glerror();

	if (old_name != 0)
	{
		sGlobalTextureMemoryInBytes -= mTextureMemory;

		if(gAuditTexture)
		{
			decTextureCounter(mTextureMemory, mComponents, mCategory) ;
		}

		LLImageGL::deleteTextures(1, &old_name);

		stop_glerror();
	}

	mTextureMemory = getMipBytes(discard_level);
	sGlobalTextureMemoryInBytes += mTextureMemory;

	if(gAuditTexture)
	{
		incTextureCounter(mTextureMemory, mComponents, mCategory) ;
	}
	// mark this as bound at this point, so we don't throw it out immediately
	mLastBindTime = sLastFrameTime;
	return TRUE;
}

BOOL LLImageGL::readBackRaw(S32 discard_level, LLImageRaw* imageraw, bool compressed_ok)
{
	// VWR-13505 : Merov : Allow gl texture read back so save texture works again (temporary)
	//llassert_always(sAllowReadBackRaw) ;
	//LL_ERRS() << "should not call this function!" << LL_ENDL ;
	
	if (discard_level < 0)
	{
		discard_level = mCurrentDiscardLevel;
		if(discard_level < 0)
			return FALSE;
	}
	
	if (mTexName == 0 || discard_level < mCurrentDiscardLevel || discard_level > mMaxDiscardLevel )
	{
		return FALSE;
	}

	S32 gl_discard = discard_level - mCurrentDiscardLevel;

	//explicitly unbind texture 
	gGL.getTexUnit(0)->unbind(mBindTarget);
	llverify(gGL.getTexUnit(0)->bindManual(mBindTarget, mTexName));	

	//debug code, leave it there commented.
	checkTexSize() ;

	LLGLint glwidth = 0;
	glGetTexLevelParameteriv(mTarget, gl_discard, GL_TEXTURE_WIDTH, (GLint*)&glwidth);
	if (glwidth == 0)
	{
		// No mip data smaller than current discard level
		return FALSE;
	}
	
	S32 width = getWidth(discard_level);
	S32 height = getHeight(discard_level);
	S32 ncomponents = getComponents();
	if (ncomponents == 0)
	{
		return FALSE;
	}
	if(width < glwidth)
	{
		LL_WARNS() << "texture size is smaller than it should be." << LL_ENDL ;
		LL_WARNS() << "width: " << width << " glwidth: " << glwidth << " mWidth: " << mWidth << 
			" mCurrentDiscardLevel: " << (S32)mCurrentDiscardLevel << " discard_level: " << (S32)discard_level << LL_ENDL ;
		return FALSE ;
	}

	if (width <= 0 || width > 2048 || height <= 0 || height > 2048 || ncomponents < 1 || ncomponents > 4)
	{
		LL_ERRS() << llformat("LLImageGL::readBackRaw: bogus params: %d x %d x %d",width,height,ncomponents) << LL_ENDL;
	}
	
	LLGLint is_compressed = 0;
	if (compressed_ok)
	{
		glGetTexLevelParameteriv(mTarget, is_compressed, GL_TEXTURE_COMPRESSED, (GLint*)&is_compressed);
	}
	
	//-----------------------------------------------------------------------------------------------
	GLenum error ;
	while((error = glGetError()) != GL_NO_ERROR)
	{
		LL_WARNS() << "GL Error happens before reading back texture. Error code: " << error << LL_ENDL ;
	}
	//-----------------------------------------------------------------------------------------------

	if (is_compressed)
	{
		LLGLint glbytes;
		glGetTexLevelParameteriv(mTarget, gl_discard, GL_TEXTURE_COMPRESSED_IMAGE_SIZE, (GLint*)&glbytes);
		if(!imageraw->allocateDataSize(width, height, ncomponents, glbytes))
		{
			LL_WARNS() << "Memory allocation failed for reading back texture. Size is: " << glbytes << LL_ENDL ;
			LL_WARNS() << "width: " << width << "height: " << height << "components: " << ncomponents << LL_ENDL ;
			return FALSE ;
		}

		glGetCompressedTexImageARB(mTarget, gl_discard, (GLvoid*)(imageraw->getData()));		
		//stop_glerror();
	}
	else
	{
		if(!imageraw->allocateDataSize(width, height, ncomponents))
		{
			LL_WARNS() << "Memory allocation failed for reading back texture." << LL_ENDL ;
			LL_WARNS() << "width: " << width << "height: " << height << "components: " << ncomponents << LL_ENDL ;
			return FALSE ;
		}
		
		glGetTexImage(GL_TEXTURE_2D, gl_discard, mFormatPrimary, mFormatType, (GLvoid*)(imageraw->getData()));		
		//stop_glerror();
	}
		
	//-----------------------------------------------------------------------------------------------
	if((error = glGetError()) != GL_NO_ERROR)
	{
		LL_WARNS() << "GL Error happens after reading back texture. Error code: " << error << LL_ENDL ;
		imageraw->deleteData() ;

		while((error = glGetError()) != GL_NO_ERROR)
		{
			LL_WARNS() << "GL Error happens after reading back texture. Error code: " << error << LL_ENDL ;
		}

		return FALSE ;
	}
	//-----------------------------------------------------------------------------------------------

	return TRUE ;
}

void LLImageGL::deleteDeadTextures()
{
	bool reset = false;

	if (reset)
	{
		gGL.getTexUnit(0)->activate();
	}
}
		
void LLImageGL::destroyGLTexture()
{
	if (mTexName != 0)
	{
		if(mTextureMemory)
		{
			if(gAuditTexture)
			{
				decTextureCounter(mTextureMemory, mComponents, mCategory) ;
			}
			sGlobalTextureMemoryInBytes -= mTextureMemory;
			mTextureMemory = 0;
		}
		
		LLImageGL::deleteTextures(1, &mTexName);			
		mCurrentDiscardLevel = -1 ; //invalidate mCurrentDiscardLevel.
		mTexName = 0;		
		mGLTextureCreated = FALSE ;
	}
}

//force to invalidate the gl texture, most likely a sculpty texture
void LLImageGL::forceToInvalidateGLTexture()
{
	if (mTexName != 0)
	{
		destroyGLTexture();
	}
	else
	{
		mCurrentDiscardLevel = -1 ; //invalidate mCurrentDiscardLevel.
	}
}

//----------------------------------------------------------------------------

void LLImageGL::setAddressMode(LLTexUnit::eTextureAddressMode mode)
{
	if (mAddressMode != mode)
	{
		mTexOptionsDirty = true;
		mAddressMode = mode;
	}

	if (gGL.getTexUnit(gGL.getCurrentTexUnitIndex())->getCurrTexture() == mTexName)
	{
		gGL.getTexUnit(gGL.getCurrentTexUnitIndex())->setTextureAddressMode(mode);
		mTexOptionsDirty = false;
	}
}

void LLImageGL::setFilteringOption(LLTexUnit::eTextureFilterOptions option)
{
	if (mFilterOption != option)
	{
		mTexOptionsDirty = true;
		mFilterOption = option;
	}

	if (mTexName != 0 && gGL.getTexUnit(gGL.getCurrentTexUnitIndex())->getCurrTexture() == mTexName)
	{
		gGL.getTexUnit(gGL.getCurrentTexUnitIndex())->setTextureFilteringOption(option);
		mTexOptionsDirty = false;
		stop_glerror();
	}
}

BOOL LLImageGL::getIsResident(BOOL test_now)
{
	if (test_now)
	{
		if (mTexName != 0)
		{
			glAreTexturesResident(1, (GLuint*)&mTexName, &mIsResident);
		}
		else
		{
			mIsResident = FALSE;
		}
	}

	return mIsResident;
}

S32 LLImageGL::getHeight(S32 discard_level) const
{
	if (discard_level < 0)
	{
		discard_level = mCurrentDiscardLevel;
	}
	S32 height = mHeight >> discard_level;
	if (height < 1) height = 1;
	return height;
}

S32 LLImageGL::getWidth(S32 discard_level) const
{
	if (discard_level < 0)
	{
		discard_level = mCurrentDiscardLevel;
	}
	S32 width = mWidth >> discard_level;
	if (width < 1) width = 1;
	return width;
}

S32 LLImageGL::getBytes(S32 discard_level) const
{
	if (discard_level < 0)
	{
		discard_level = mCurrentDiscardLevel;
	}
	S32 w = mWidth>>discard_level;
	S32 h = mHeight>>discard_level;
	if (w == 0) w = 1;
	if (h == 0) h = 1;
	return dataFormatBytes(mFormatPrimary, w, h);
}

S32 LLImageGL::getMipBytes(S32 discard_level) const
{
	if (discard_level < 0)
	{
		discard_level = mCurrentDiscardLevel;
	}
	S32 w = mWidth>>discard_level;
	S32 h = mHeight>>discard_level;
	S32 res = dataFormatBytes(mFormatPrimary, w, h);
	if (mUseMipMaps)
	{
		while (w > 1 && h > 1)
		{
			w >>= 1; if (w == 0) w = 1;
			h >>= 1; if (h == 0) h = 1;
			res += dataFormatBytes(mFormatPrimary, w, h);
		}
	}
	return res;
}

BOOL LLImageGL::isJustBound() const
{
	return (BOOL)(sLastFrameTime - mLastBindTime < 0.5f);
}

BOOL LLImageGL::getBoundRecently() const
{
	return (BOOL)(sLastFrameTime - mLastBindTime < MIN_TEXTURE_LIFETIME);
}

void LLImageGL::setTarget(const LLGLenum target, const LLTexUnit::eTextureType bind_target)
{
	mTarget = target;
	mBindTarget = bind_target;
}

//Used by media in V2
void LLImageGL::setNeedsAlphaAndPickMask(BOOL need_mask) 
{
	if(mNeedsAlphaAndPickMask != need_mask)
	{
		mNeedsAlphaAndPickMask = need_mask;

		if(mNeedsAlphaAndPickMask)
		{
			mAlphaOffset = 0 ;
		}
		else //do not need alpha mask
		{
			mAlphaOffset = INVALID_OFFSET ;
		}
	}
}

void LLImageGL::calcAlphaChannelOffsetAndStride()
{
	if(mAlphaOffset == INVALID_OFFSET)//do not need alpha mask
	{
		return ;
	}

	mAlphaStride = -1 ;
	switch (mFormatPrimary)
	{
	case GL_LUMINANCE:
	case GL_ALPHA:
		mAlphaStride = 1;
		break;
	case GL_LUMINANCE_ALPHA:
		mAlphaStride = 2;
		break;
	case GL_RGB:
		setNeedsAlphaAndPickMask(FALSE);
		return ; //no alpha channel.
	case GL_RGBA:
		mAlphaStride = 4;
		break;
	case GL_BGRA_EXT:
		mAlphaStride = 4;
		break;
	default:
		break;
	}

	mAlphaOffset = -1 ;
	if (mFormatType == GL_UNSIGNED_BYTE)
	{
		mAlphaOffset = mAlphaStride - 1 ;
	}
	else if(is_little_endian())
	{
		if (mFormatType == GL_UNSIGNED_INT_8_8_8_8)
		{
			mAlphaOffset = 0 ;
		}
		else if (mFormatType == GL_UNSIGNED_INT_8_8_8_8_REV)
		{
			mAlphaOffset = 3 ;
		}
	}
	else //big endian
	{
		if (mFormatType == GL_UNSIGNED_INT_8_8_8_8)
		{
			mAlphaOffset = 3 ;
		}
		else if (mFormatType == GL_UNSIGNED_INT_8_8_8_8_REV)
		{
			mAlphaOffset = 0 ;
		}
	}

	if( mAlphaStride < 1 || //unsupported format
		mAlphaOffset < 0 || //unsupported type
		(mFormatPrimary == GL_BGRA_EXT && mFormatType != GL_UNSIGNED_BYTE)) //unknown situation
	{
		LL_WARNS() << "Cannot analyze alpha for image with format type " << std::hex << mFormatType << std::dec << LL_ENDL;

		setNeedsAlphaAndPickMask(FALSE);
	}
}

//std::map<LLGLuint, std::list<std::pair<std::string,std::string> > > sTextureMaskMap;
void LLImageGL::analyzeAlpha(const void* data_in, U32 w, U32 h)
{
	if(!mNeedsAlphaAndPickMask)
	{
		return ;
	}

	F64 sum = 0;

	U32 length = w * h;
	U32 alphatotal = 0;
	
	U32 sample[16];
	memset(sample, 0, sizeof(U32)*16);

	// generate histogram of quantized alpha.
	// also add-in the histogram of a 2x2 box-sampled version.  The idea is
	// this will mid-skew the data (and thus increase the chances of not
	// being used as a mask) from high-frequency alpha maps which
	// suffer the worst from aliasing when used as alpha masks.
	if (w >= 2 && h >= 2)
	{
		llassert(w%2 == 0);
		llassert(h%2 == 0);
		const GLubyte* rowstart = ((const GLubyte*) data_in) + mAlphaOffset;
		for (U32 y = 0; y < h; y+=2)
		{
			const GLubyte* current = rowstart;
			for (U32 x = 0; x < w; x+=2)
			{
				const U32 s1 = current[0];
				alphatotal += s1;
				const U32 s2 = current[w * mAlphaStride];
				alphatotal += s2;
				current += mAlphaStride;
				const U32 s3 = current[0];
				alphatotal += s3;
				const U32 s4 = current[w * mAlphaStride];
				alphatotal += s4;
				current += mAlphaStride;

				++sample[s1/16];
				++sample[s2/16];
				++sample[s3/16];
				++sample[s4/16];

				const U32 asum = (s1+s2+s3+s4);
				alphatotal += asum;
				sample[asum/(16*4)] += 4;

				S32 avg = (s1+s2+s3+s4)/4;
				if(avg >=128)
				{
					avg-=255;
				}
				sum+=F64(avg*avg*4)/F64(length);
			}
			
			
			rowstart += 2 * w * mAlphaStride;
		}

		length *= 2; // we sampled everything twice, essentially
	}
	else
	{
		const GLubyte* current = ((const GLubyte*) data_in) + mAlphaOffset;
		for (U32 i = 0; i < length; i++)
		{
			const U32 s1 = *current;
			alphatotal += s1;
			++sample[s1/16];
			current += mAlphaStride;

			if(i%2==0)
			{
				S32 avg = (s1+current[mAlphaStride])/2;
				if(avg >=128)
					avg-=255;
				sum+=F64(avg*avg*2)/F64(length);
			}
		}
	}

	// if more than 1/16th of alpha samples are mid-range, this
	// shouldn't be treated as a 1-bit mask

	// also, if all of the alpha samples are clumped on one half
	// of the range (but not at an absolute extreme), then consider
	// this to be an intentional effect and don't treat as a mask.

	U32 midrangetotal = 0;
	for (U32 i = 3; i < 13; i++)
	{
		midrangetotal += sample[i];
	}
	U32 lowerhalftotal = 0;
	for (U32 i = 0; i < 8; i++)
	{
		lowerhalftotal += sample[i];
	}
	U32 upperhalftotal = 0;
	for (U32 i = 8; i < 16; i++)
	{
		upperhalftotal += sample[i];
	}

	if (midrangetotal > length/48 || // lots of midrange, or
	    (lowerhalftotal == length && alphatotal != 0) || // all close to transparent but not all totally transparent, or
	    (upperhalftotal == length && alphatotal != 255*length)) // all close to opaque but not all totally opaque
	{
		mIsMask = FALSE; // not suitable for masking
	}
	else
	{
		mIsMask = TRUE;
	}

	mMaskRMSE = sqrt(sum)/255.0;
	
	/*std::list<std::pair<std::string,std::string> > &data = sTextureMaskMap[getTexName()];
	data.clear();
	data.push_back(std::make_pair("RMSE", llformat("%f",mMaskRMSE)));
	data.push_back(std::make_pair(" sum", llformat("%lf",sum)));
	data.push_back(std::make_pair(" n", llformat("%u",h*w)));
	data.push_back(std::make_pair("legacymask", mIsMask ? "TRUE" : "FALSE"));
	data.push_back(std::make_pair(" index", llformat("%u",getTexName())));
	data.push_back(std::make_pair(" length", llformat("%u",length)));
	data.push_back(std::make_pair(" stride", llformat("%i",S32(mAlphaOffset))));
	data.push_back(std::make_pair(" split", llformat("%u|%u|%u",lowerhalftotal,midrangetotal,upperhalftotal)));
	data.push_back(std::make_pair(" alphatotal", llformat("%u",alphatotal)));
	data.push_back(std::make_pair(" alphatotal/48", llformat("%u",length/48)));*/
	
}

//----------------------------------------------------------------------------
void LLImageGL::updatePickMask(S32 width, S32 height, const U8* data_in)
{
	if(!mNeedsAlphaAndPickMask)
	{
		return ;
	}

	delete [] mPickMask;
	mPickMask = NULL;
	mPickMaskWidth = mPickMaskHeight = 0;

	if (mFormatType != GL_UNSIGNED_BYTE ||
	    mFormatPrimary != GL_RGBA)
	{
		//cannot generate a pick mask for this texture
		return;
	}

	U32 pick_width = width/2 + 1;
	U32 pick_height = height/2 + 1;

	U32 size = pick_width * pick_height;
	size = (size + 7) / 8; // pixelcount-to-bits
	mPickMask = new U8[size];
	mPickMaskWidth = pick_width - 1;
	mPickMaskHeight = pick_height - 1;

	memset(mPickMask, 0, sizeof(U8) * size);

	U32 pick_bit = 0;
	
	for (S32 y = 0; y < height; y += 2)
	{
		for (S32 x = 0; x < width; x += 2)
		{
			U8 alpha = data_in[(y*width+x)*4+3];

			if (alpha > 32)
			{
				U32 pick_idx = pick_bit/8;
				U32 pick_offset = pick_bit%8;
				llassert(pick_idx < size);

				mPickMask[pick_idx] |= 1 << pick_offset;
			}
			
			++pick_bit;
		}
	}
}

BOOL LLImageGL::getMask(const LLVector2 &tc)
{
	BOOL res = TRUE;

	if (mPickMask)
	{
		F32 u,v;
		if (LL_LIKELY(tc.isFinite()))
		{
			u = tc.mV[0] - floorf(tc.mV[0]);
			v = tc.mV[1] - floorf(tc.mV[1]);
		}
		else
		{
			LL_WARNS_ONCE("render") << "Ugh, non-finite u/v in mask pick" << LL_ENDL;
			u = v = 0.f;
			// removing assert per EXT-4388
			// llassert(false);
		}

		if (LL_UNLIKELY(u < 0.f || u > 1.f ||
				v < 0.f || v > 1.f))
		{
			LL_WARNS_ONCE("render") << "Ugh, u/v out of range in image mask pick" << LL_ENDL;
			u = v = 0.f;
			// removing assert per EXT-4388
			// llassert(false);
		}

		S32 x = llfloor(u * mPickMaskWidth);
		S32 y = llfloor(v * mPickMaskHeight);

		if (LL_UNLIKELY(x > mPickMaskWidth))
		{
			LL_WARNS_ONCE("render") << "Ooh, width overrun on pick mask read, that coulda been bad." << LL_ENDL;
			x = llmax((U16)0, mPickMaskWidth);
		}
		if (LL_UNLIKELY(y > mPickMaskHeight))
		{
			LL_WARNS_ONCE("render") << "Ooh, height overrun on pick mask read, that woulda been bad." << LL_ENDL;
			y = llmax((U16)0, mPickMaskHeight);
		}

		S32 idx = y*mPickMaskWidth+x;
		S32 offset = idx%8;

		res = mPickMask[idx/8] & (1 << offset) ? TRUE : FALSE;
	}
	
	return res;
}

void LLImageGL::setCategory(S32 category) 
{
	if(!gAuditTexture)
	{
		return ;
	}
	if(mCategory != category)
	{		
		if(mCategory > -1)
		{
			sTextureMemByCategory[mCategory] -= mTextureMemory ;
		}
		if(category > -1 && category < sMaxCategories)
		{
			sTextureMemByCategory[category] += mTextureMemory ;		
			mCategory = category;
		}
		else
		{
			mCategory = -1 ;
		}
	}
}

//for debug use 
//val is a "power of two" number
S32 LLImageGL::getTextureCounterIndex(U32 val) 
{
	//index range is [0, MAX_TEXTURE_LOG_SIZE].
	if(val < 2)
	{
		return 0 ;
	}
	else if(val >= (1 << MAX_TEXTURE_LOG_SIZE))
	{
		return MAX_TEXTURE_LOG_SIZE ;
	}
	else
	{
		S32 ret = 0 ;
		while(val >>= 1)
		{
			++ret;
		}
		return ret ;
	}
}

//static
void LLImageGL::incTextureCounter(U32 val, S32 ncomponents, S32 category) 
{
	sTextureLoadedCounter[getTextureCounterIndex(val)]++ ;
	if(category > -1)
		sTextureMemByCategory[category] += (S32)val * ncomponents ;
}

//static
void LLImageGL::decTextureCounter(U32 val, S32 ncomponents, S32 category) 
{
	sTextureLoadedCounter[getTextureCounterIndex(val)]-- ;
	if(category > -1)
		sTextureMemByCategory[category] -= (S32)val * ncomponents ;
}

void LLImageGL::setCurTexSizebar(S32 index, BOOL set_pick_size)
{
	sCurTexSizeBar = index ;

	if(set_pick_size)
	{
		sCurTexPickSize = (1 << index) ;
	}
	else
	{
		sCurTexPickSize = -1 ;
	}
}
void LLImageGL::resetCurTexSizebar()
{
	sCurTexSizeBar = -1 ;
	sCurTexPickSize = -1 ;
}
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------


// Manual Mip Generation
/*
		S32 width = getWidth(discard_level);
		S32 height = getHeight(discard_level);
		S32 w = width, h = height;
		S32 nummips = 1;
		while (w > 4 && h > 4)
		{
			w >>= 1; h >>= 1;
			nummips++;
		}
		stop_glerror();
		w = width, h = height;
		const U8* prev_mip_data = 0;
		const U8* cur_mip_data = 0;
		for (int m=0; m<nummips; m++)
		{
			if (m==0)
			{
				cur_mip_data = rawdata;
			}
			else
			{
				S32 bytes = w * h * mComponents;
				U8* new_data = new U8[bytes];
				LLImageBase::generateMip(prev_mip_data, new_data, w, h, mComponents);
				cur_mip_data = new_data;
			}
			llassert(w > 0 && h > 0 && cur_mip_data);
			U8 test = cur_mip_data[w*h*mComponents-1];
			{
				LLImageGL::setManualImage(mTarget, m, mFormatInternal, w, h, mFormatPrimary, mFormatType, cur_mip_data);
				stop_glerror();
			}
			if (prev_mip_data && prev_mip_data != rawdata)
			{
				delete prev_mip_data;
			}
			prev_mip_data = cur_mip_data;
			w >>= 1;
			h >>= 1;
		}
		if (prev_mip_data && prev_mip_data != rawdata)
		{
			delete prev_mip_data;
		}
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL,  nummips);
*/  
