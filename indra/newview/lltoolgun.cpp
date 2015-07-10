/** 
 * @file lltoolgun.cpp
 * @brief LLToolGun class implementation
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "lltoolgun.h"

#include "llviewerwindow.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llviewercontrol.h"
#include "llsky.h"
#include "llappviewer.h"
#include "llresmgr.h"
#include "llfontgl.h"
#include "llui.h"
#include "llviewertexturelist.h"
#include "llviewercamera.h"
#include "llhudmanager.h"
#include "lltoolmgr.h"
#include "lltoolgrab.h"
#include "lluiimage.h"
// Linden library includes
#include "llwindow.h"			// setMouseClipping()

bool getCustomColorRLV(const LLUUID& id, LLColor4& color, LLViewerRegion* parent_estate, bool name_restricted);
#include "llavatarnamecache.h"
#include "llworld.h"
#include "rlvhandler.h"

LLToolGun::LLToolGun( LLToolComposite* composite )
:	LLTool( std::string("gun"), composite ),
		mIsSelected(FALSE)
{
	mCrosshairp = LLUI::getUIImage("UIImgCrosshairsUUID"); // <alchemy/> - UI Caching
}

void LLToolGun::handleSelect()
{
// [RLVa:KB] - Checked: 2014-02-24 (RLVa-1.4.10)
	if (gFocusMgr.getAppHasFocus())
	{
// [/RLVa:KB]
		gViewerWindow->hideCursor();
		gViewerWindow->moveCursorToCenter();
		gViewerWindow->getWindow()->setMouseClipping(TRUE);
		mIsSelected = TRUE;
// [RLVa:KB] - Checked: 2014-02-24 (RLVa-1.4.10)
	}
// [/RLVa:KB]
}

void LLToolGun::handleDeselect()
{
	gViewerWindow->moveCursorToCenter();
	gViewerWindow->showCursor();
	gViewerWindow->getWindow()->setMouseClipping(FALSE);
	mIsSelected = FALSE;
}

BOOL LLToolGun::handleMouseDown(S32 x, S32 y, MASK mask)
{
	gGrabTransientTool = this;
	LLToolMgr::getInstance()->getCurrentToolset()->selectTool( LLToolGrab::getInstance() );

	return LLToolGrab::getInstance()->handleMouseDown(x, y, mask);
}

BOOL LLToolGun::handleHover(S32 x, S32 y, MASK mask) 
{
	if( gAgentCamera.cameraMouselook() && mIsSelected )
	{
		const F32 NOMINAL_MOUSE_SENSITIVITY = 0.0025f;

		static LLCachedControl<F32> mouse_sensitivity_setting(gSavedSettings, "MouseSensitivity");
		F32 mouse_sensitivity = clamp_rescale(mouse_sensitivity_setting, 0.f, 15.f, 0.5f, 2.75f) * NOMINAL_MOUSE_SENSITIVITY;

		// ...move the view with the mouse

		// get mouse movement delta
		S32 dx = -gViewerWindow->getCurrentMouseDX();
		S32 dy = -gViewerWindow->getCurrentMouseDY();
		
		if (dx != 0 || dy != 0)
		{
			// ...actually moved off center
			const F32 fov = LLViewerCamera::getInstance()->getView() / DEFAULT_FIELD_OF_VIEW;
			static LLCachedControl<bool> invert_mouse(gSavedSettings, "InvertMouse");
			if (invert_mouse)
			{
				gAgent.pitch(mouse_sensitivity * fov * -dy);
			}
			else
			{
				gAgent.pitch(mouse_sensitivity * fov * dy);
			}
			LLVector3 skyward = gAgent.getReferenceUpVector();
			gAgent.rotate(mouse_sensitivity * fov * dx, skyward.mV[VX], skyward.mV[VY], skyward.mV[VZ]);

			static LLCachedControl<bool> mouse_sun(gSavedSettings, "MouseSun");
			if (mouse_sun)
			{
				gSky.setSunDirection(LLViewerCamera::getInstance()->getAtAxis(), LLVector3(0.f, 0.f, 0.f));
				gSky.setOverrideSun(TRUE);
				gSavedSettings.setVector3("SkySunDefaultPosition", LLViewerCamera::getInstance()->getAtAxis());
			}

			gViewerWindow->moveCursorToCenter();
			gViewerWindow->hideCursor();
		}

		LL_DEBUGS("UserInput") << "hover handled by LLToolGun (mouselook)" << LL_ENDL;
	}
	else
	{
		LL_DEBUGS("UserInput") << "hover handled by LLToolGun (not mouselook)" << LL_ENDL;
	}

	// HACK to avoid assert: error checking system makes sure that the cursor is set during every handleHover.  This is actually a no-op since the cursor is hidden.
	gViewerWindow->setCursor(UI_CURSOR_ARROW);  

	return TRUE;
}

void LLToolGun::draw()
{
	static LLCachedControl<bool> show_crosshairs(gSavedSettings, "ShowCrosshairs");
	static LLCachedControl<bool> show_iff(gSavedSettings, "AlchemyMouselookIFF", true);
	static LLCachedControl<F32> iff_range(gSavedSettings, "AlchemyMouselookIFFRange", 380.f);
	if (show_crosshairs)
	{
		const S32 windowWidth = gViewerWindow->getWorldViewRectScaled().getWidth();
		const S32 windowHeight = gViewerWindow->getWorldViewRectScaled().getHeight();
		static const LLCachedControl<LLColor4> color("LiruCrosshairColor");
		LLColor4 targetColor = color;
		targetColor.mV[VALPHA] = 0.5f;
		if (show_iff && !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWMINIMAP))
		{
			LLVector3d myPosition = gAgentCamera.getCameraPositionGlobal();
			LLQuaternion myRotation = LLViewerCamera::getInstance()->getQuaternion();
			myRotation.set(-myRotation.mQ[VX], -myRotation.mQ[VY], -myRotation.mQ[VZ], myRotation.mQ[VW]);

			bool no_names(gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMETAGS));
			bool name_restricted = no_names || gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES);

			LLWorld::pos_map_t positions;
			LLWorld& world(LLWorld::instance());
			world.getAvatars(&positions, gAgent.getPositionGlobal(), name_restricted && gRlvHandler.hasBehaviour(RLV_BHVR_CAMAVDIST) ? llmin(iff_range(), gRlvHandler.camPole(RLV_BHVR_CAMAVDIST)) : iff_range);
			for (LLWorld::pos_map_t::const_iterator iter = positions.cbegin(), iter_end = positions.cend(); iter != iter_end; ++iter)
			{
				const LLUUID& id = iter->first;
				const LLVector3d& targetPosition = iter->second;
				if (id == gAgentID || targetPosition.isNull())
				{
					continue;
				}

				LLVector3d magicVector = (targetPosition - myPosition) * myRotation;
				magicVector.setVec(-magicVector.mdV[VY], magicVector.mdV[VZ], magicVector.mdV[VX]);
				if (magicVector.mdV[VX] > -0.75 && magicVector.mdV[VX] < 0.75 && magicVector.mdV[VZ] > 0.0 && magicVector.mdV[VY] > -1.5 && magicVector.mdV[VY] < 1.5) // Do not fuck with these, cheater. :(
				{
					LLAvatarName avatarName;
					if (!no_names)
						LLAvatarNameCache::get(id, &avatarName);
					getCustomColorRLV(id, targetColor, world.getRegionFromPosGlobal(targetPosition), name_restricted);
					const std::string name(no_names ? LLStringUtil::null : name_restricted ? RlvStrings::getAnonym(avatarName.getNSName()) : avatarName.getNSName());
					targetColor.mV[VALPHA] = 0.5f;
					LLFontGL::getFontSansSerifBold()->renderUTF8(
						llformat("%s : %.2fm", name.c_str(), (targetPosition - myPosition).magVec()),
						0, (windowWidth / 2.f), (windowHeight / 2.f) - 25.f, targetColor,
						LLFontGL::HCENTER, LLFontGL::TOP, LLFontGL::BOLD, LLFontGL::NO_SHADOW
						);

					break;
				}
			}
		}

		mCrosshairp->draw(
			(windowWidth - mCrosshairp->getWidth() ) / 2,
			(windowHeight - mCrosshairp->getHeight() ) / 2, targetColor);
	}
}
