#include "global.h"
/*
-----------------------------------------------------------------------------
 Class: NoteField

 Desc: See header.

 Copyright (c) 2001-2002 by the person(s) listed below.  All rights reserved.
	Chris Danford
-----------------------------------------------------------------------------
*/

#include "NoteField.h"
#include "RageUtil.h"
#include "GameConstantsAndTypes.h"
#include "PrefsManager.h"
#include "ArrowEffects.h"
#include "PrefsManager.h"
#include "GameManager.h"
#include "GameState.h"
#include "RageException.h"
#include "RageTimer.h"
#include "RageLog.h"
#include <math.h>
#include "ThemeManager.h"


const float HOLD_NOTE_BITS_PER_BEAT	= 6;
const float HOLD_NOTE_BITS_PER_ROW	= HOLD_NOTE_BITS_PER_BEAT / ROWS_PER_BEAT;
const float ROWS_BETWEEN_HOLD_BITS	= 1 / HOLD_NOTE_BITS_PER_ROW;	

NoteField::NoteField()
{
	m_rectMeasureBar.TurnShadowOff();

	m_textMeasureNumber.LoadFromFont( THEME->GetPathTo("Fonts","normal") );
	m_textMeasureNumber.SetZoom( 1.0f );

	m_rectMarkerBar.TurnShadowOff();
	m_rectMarkerBar.SetEffectDiffuseShift( 2, RageColor(1,1,1,0.5f), RageColor(0.5f,0.5f,0.5f,0.5f) );

	m_fBeginMarker = m_fEndMarker = -1;

	m_fPercentFadeToFail = -1;
}


void NoteField::Load( NoteData* pNoteData, PlayerNumber pn, int iFirstPixelToDraw, int iLastPixelToDraw )
{
	m_PlayerNumber = pn;
	m_iFirstPixelToDraw = iFirstPixelToDraw;
	m_iLastPixelToDraw = iLastPixelToDraw;

	m_fPercentFadeToFail = -1;

	NoteDataWithScoring::Init();

	m_bIsHoldingHoldNote.clear();
	m_bIsHoldingHoldNote.insert(m_bIsHoldingHoldNote.end(), pNoteData->GetNumTapNotes(), false);

	this->CopyAll( pNoteData );

	// init note displays
	for( int c=0; c<GetNumTracks(); c++ ) 
		m_NoteDisplay[c].Load( c, pn );

	ASSERT( GetNumTracks() == GAMESTATE->GetCurrentStyleDef()->m_iColsPerPlayer );
}

void NoteField::Update( float fDeltaTime )
{
	m_rectMarkerBar.Update( fDeltaTime );

	if( m_fPercentFadeToFail >= 0 )
		m_fPercentFadeToFail = min( m_fPercentFadeToFail + fDeltaTime/1.5f, 1 );	// take 1.5 seconds to totally fade
}

int NoteField::GetWidth()
{
	return (GetNumTracks()+1) * ARROW_SIZE;
}

void NoteField::DrawBeatBar( const float fBeat )
{
	bool bIsMeasure = fmodf( fBeat, (float)BEATS_PER_MEASURE ) == 0;
	int iMeasureIndex = (int)fBeat / BEATS_PER_MEASURE;
	int iMeasureNoDisplay = iMeasureIndex+1;

	NoteType nt = BeatToNoteType( fBeat );

	const float fYOffset	= ArrowGetYOffset(			m_PlayerNumber, fBeat );
	const float fYPos		= ArrowGetYPos(	m_PlayerNumber, fYOffset );


	int iSegWidth;
	int iSpaceWidth;
	float fBrightness;
	float fScrollSpeed = GAMESTATE->m_PlayerOptions[m_PlayerNumber].m_fScrollSpeed;
	switch( nt )
	{
	default:	ASSERT(0);
	case NOTE_TYPE_4TH:	iSegWidth = 16; iSpaceWidth = 0; fBrightness = 1;	break;
	case NOTE_TYPE_8TH:	iSegWidth = 12; iSpaceWidth = 4; fBrightness = SCALE(fScrollSpeed,1.f,2.f,0.f,1.f);	break;
	case NOTE_TYPE_16TH:iSegWidth = 4;  iSpaceWidth = 4; fBrightness = SCALE(fScrollSpeed,2.f,4.f,0.f,1.f);	break;
	}
	CLAMP( fBrightness, 0, 1 );

	int iWidth = GetWidth();
	for( int i=-iWidth/2; i<+iWidth/2; )
	{
		m_rectMeasureBar.StretchTo( RectI(i,0,i+iSegWidth,0) );
		m_rectMeasureBar.SetY( fYPos );
		m_rectMeasureBar.SetZoomY( bIsMeasure ? 6.f : 3.f );
		m_rectMeasureBar.SetDiffuse( RageColor(1,1,1,0.5f*fBrightness) );
		m_rectMeasureBar.Draw();

		i += iSegWidth + iSpaceWidth;
	}

	if( bIsMeasure )
	{
		m_textMeasureNumber.SetDiffuse( RageColor(1,1,1,1) );
		m_textMeasureNumber.SetGlow( RageColor(1,1,1,0) );
		m_textMeasureNumber.SetText( ssprintf("%d", iMeasureNoDisplay) );
		m_textMeasureNumber.SetXY( -iWidth/2.f + 10, fYPos );
		m_textMeasureNumber.Draw();
	}
}

void NoteField::DrawMarkerBar( const float fBeat )
{
	const float fYOffset	= ArrowGetYOffset(			m_PlayerNumber, fBeat );
	const float fYPos		= ArrowGetYPos(	m_PlayerNumber, fYOffset );

	m_rectMarkerBar.SetXY( 0, fYPos );
	m_rectMarkerBar.SetZoomX( (float)(GetNumTracks()+1) * ARROW_SIZE );
	m_rectMarkerBar.SetZoomY( (float)ARROW_SIZE );
	m_rectMarkerBar.Draw();
}

void NoteField::DrawAreaHighlight( const float fStartBeat, const float fEndBeat )
{
	const float fYStartOffset	= ArrowGetYOffset( m_PlayerNumber, fStartBeat );
	const float fYStartPos		= ArrowGetYPos(	m_PlayerNumber, fYStartOffset );
	const float fYEndOffset		= ArrowGetYOffset( m_PlayerNumber, fEndBeat );
	const float fYEndPos		= ArrowGetYPos(	m_PlayerNumber, fYEndOffset );

	m_rectAreaHighlight.StretchTo( RectI(0, (int)fYStartPos-ARROW_SIZE/2, 1, (int)fYEndPos+ARROW_SIZE/2) );
	m_rectAreaHighlight.SetZoomX( (float)(GetNumTracks()+1) * ARROW_SIZE );
	m_rectAreaHighlight.SetDiffuse( RageColor(1,0,0,0.3f) );
	m_rectAreaHighlight.Draw();
}



void NoteField::DrawBPMText( const float fBeat, const float fBPM )
{
	const float fYOffset	= ArrowGetYOffset(			m_PlayerNumber, fBeat );
	const float fYPos		= ArrowGetYPos(	m_PlayerNumber, fYOffset );

	m_textMeasureNumber.SetDiffuse( RageColor(1,0,0,1) );
	m_textMeasureNumber.SetGlow( RageColor(1,1,1,cosf(RageTimer::GetTimeSinceStart()*2)/2+0.5f) );
	m_textMeasureNumber.SetText( ssprintf("%.2f", fBPM) );
	m_textMeasureNumber.SetXY( -GetWidth()/2.f - 60, fYPos );
	m_textMeasureNumber.Draw();
}

void NoteField::DrawFreezeText( const float fBeat, const float fSecs )
{
	const float fYOffset	= ArrowGetYOffset(			m_PlayerNumber, fBeat );
	const float fYPos		= ArrowGetYPos(	m_PlayerNumber, fYOffset );

	m_textMeasureNumber.SetDiffuse( RageColor(0.8f,0.8f,0,1) );
	m_textMeasureNumber.SetGlow( RageColor(1,1,1,cosf(RageTimer::GetTimeSinceStart()*2)/2+0.5f) );
	m_textMeasureNumber.SetText( ssprintf("%.2f", fSecs) );
	m_textMeasureNumber.SetXY( -GetWidth()/2.f - 10, fYPos );
	m_textMeasureNumber.Draw();
}

void NoteField::DrawBGChangeText( const float fBeat, const CString sNewBGName )
{
	const float fYOffset	= ArrowGetYOffset(			m_PlayerNumber, fBeat );
	const float fYPos		= ArrowGetYPos(	m_PlayerNumber, fYOffset );

	m_textMeasureNumber.SetDiffuse( RageColor(0,1,0,1) );
	m_textMeasureNumber.SetGlow( RageColor(1,1,1,cosf(RageTimer::GetTimeSinceStart()*2)/2+0.5f) );
	m_textMeasureNumber.SetText( sNewBGName );
	m_textMeasureNumber.SetXY( +GetWidth()/2.f + 10, fYPos );
	m_textMeasureNumber.Draw();
}

void NoteField::DrawPrimitives()
{
	//LOG->Trace( "NoteField::DrawPrimitives()" );

	const float fSongBeat = GAMESTATE->m_fSongBeat;

	// CPU OPTIMIZATION OPPORTUNITY:
	// change this probing to binary search

	// probe for first note on the screen
	float fFirstBeatToDraw = fSongBeat-2;	// Adjust to balance of performance and showing enough notes.
	while( fFirstBeatToDraw<fSongBeat )
	{
		float fYOffset = ArrowGetYOffset(m_PlayerNumber, fFirstBeatToDraw);
		float fYPosWOReverse = ArrowGetYPosWithoutReverse(m_PlayerNumber, fYOffset );
		if( fYPosWOReverse < m_iFirstPixelToDraw )	// off screen
			fFirstBeatToDraw += 0.1f;	// move toward fSongBeat
		else	// on screen
			break;	// stop probing
	}
	fFirstBeatToDraw -= 0.1f;	// rewind if we intentionally overshot

	// probe for last note to draw
	float fLastBeatToDraw = fSongBeat+20;	// worst case is 0.25x + boost.  Adjust to balance of performance and showing enough notes.
	while( fLastBeatToDraw>fSongBeat )
	{
		float fYOffset = ArrowGetYOffset(m_PlayerNumber, fLastBeatToDraw);
		float fYPosWOReverse = ArrowGetYPosWithoutReverse(m_PlayerNumber, fYOffset );
		if( fYPosWOReverse > m_iLastPixelToDraw )	// off screen
			fLastBeatToDraw -= 0.1f;	// move toward fSongBeat
		else	// on screen
			break;	// stop probing
	}
	fLastBeatToDraw += 0.1f;	// fast forward since we intentionally overshot


	const int iFirstIndexToDraw  = BeatToNoteRow(fFirstBeatToDraw);
	const int iLastIndexToDraw   = BeatToNoteRow(fLastBeatToDraw);

//	LOG->Trace( "start = %f.1, end = %f.1", fFirstBeatToDraw-fSongBeat, fLastBeatToDraw-fSongBeat );
//	LOG->Trace( "Drawing elements %d through %d", iFirstIndexToDraw, iLastIndexToDraw );

	if( GAMESTATE->m_bEditing )
	{
		ASSERT(GAMESTATE->m_pCurSong);

		unsigned i;

		//
		// Draw beat bars
		//
		{
			float fStartDrawingMeasureBars = max( 0, froundf(fFirstBeatToDraw-0.25f,0.25f) );
			for( float f=fStartDrawingMeasureBars; f<fLastBeatToDraw; f+=0.25f )
				DrawBeatBar( f );
		}

		//
		// BPM text
		//
		vector<BPMSegment> &aBPMSegments = GAMESTATE->m_pCurSong->m_BPMSegments;
		for( i=0; i<aBPMSegments.size(); i++ )
		{
			if(aBPMSegments[i].m_fStartBeat >= fFirstBeatToDraw &&
			   aBPMSegments[i].m_fStartBeat <= fLastBeatToDraw)
				DrawBPMText( aBPMSegments[i].m_fStartBeat, aBPMSegments[i].m_fBPM );
		}
		//
		// Freeze text
		//
		vector<StopSegment> &aStopSegments = GAMESTATE->m_pCurSong->m_StopSegments;
		for( i=0; i<aStopSegments.size(); i++ )
		{
			if(aStopSegments[i].m_fStartBeat >= fFirstBeatToDraw &&
			   aStopSegments[i].m_fStartBeat <= fLastBeatToDraw)
			DrawFreezeText( aStopSegments[i].m_fStartBeat, aStopSegments[i].m_fStopSeconds );
		}

		//
		// BGChange text
		//
		vector<BackgroundChange> &aBackgroundChanges = GAMESTATE->m_pCurSong->m_BackgroundChanges;
		for( i=0; i<aBackgroundChanges.size(); i++ )
		{
			if(aBackgroundChanges[i].m_fStartBeat >= fFirstBeatToDraw &&
			   aBackgroundChanges[i].m_fStartBeat <= fLastBeatToDraw)
				DrawBGChangeText( aBackgroundChanges[i].m_fStartBeat, aBackgroundChanges[i].m_sBGName );
		}

		//
		// Draw marker bars
		//
		if( m_fBeginMarker != -1  &&  m_fEndMarker != -1 )
			DrawAreaHighlight( m_fBeginMarker, m_fEndMarker );
		else if( m_fBeginMarker != -1 )
			DrawMarkerBar( m_fBeginMarker );
		else if( m_fEndMarker != -1 )
			DrawMarkerBar( m_fEndMarker );

	}


	//
	// Optimization is very important here because there are so many arrows to draw.  
	// Draw the arrows in order of column.  This minimize texture switches and let us
	// draw in big batches.
	//

	float fSelectedRangeGlow = SCALE( cosf(RageTimer::GetTimeSinceStart()*2), -1, 1, 0.1f, 0.3f );


	for( int c=0; c<GetNumTracks(); c++ )	// for each arrow column
	{
		/////////////////////////////////
		// Draw all HoldNotes in this column (so that they appear under the tap notes)
		/////////////////////////////////
		int i;
		for( i=0; i < GetNumHoldNotes(); i++ )
		{
			const HoldNote &hn = GetHoldNote(i);
			const HoldNoteScore hns = GetHoldNoteScore(i);
			const float fLife = GetHoldNoteLife(i);
			const bool bIsHoldingNote = (i < int(m_bIsHoldingHoldNote.size()))?
				m_bIsHoldingHoldNote[i]: false;
			
			if( hns == HNS_OK )	// if this HoldNote was completed
				continue;	// don't draw anything

			if( hn.iTrack != c )	// this HoldNote doesn't belong to this column
				continue;

			// If no part of this HoldNote is on the screen, skip it
			if( !( fFirstBeatToDraw <= hn.fEndBeat && hn.fEndBeat <= fLastBeatToDraw  ||
				fFirstBeatToDraw <= hn.fStartBeat  && hn.fStartBeat <= fLastBeatToDraw  ||
				hn.fStartBeat < fFirstBeatToDraw   && hn.fEndBeat > fLastBeatToDraw ) )
			{
				continue;	// skip
			}

			bool bIsInSelectionRange = false;
			if( m_fBeginMarker!=-1 && m_fEndMarker!=-1 )
				bIsInSelectionRange = m_fBeginMarker<=hn.fStartBeat && hn.fStartBeat<=m_fEndMarker && m_fBeginMarker<=hn.fEndBeat && hn.fEndBeat<=m_fEndMarker;

			m_NoteDisplay[c].DrawHold( hn, bIsHoldingNote, fLife, bIsInSelectionRange ? fSelectedRangeGlow : m_fPercentFadeToFail );
		}
		

		///////////////////////////////////
		// Draw all TapNotes in this column
		///////////////////////////////////
		for( i=iFirstIndexToDraw; i<=iLastIndexToDraw; i++ )	//	 for each row
		{	
			if( GetTapNote(c, i) == TAP_EMPTY )	// no note here
				continue;	// skip
			
			if( GetTapNote(c, i) == TAP_HOLD_HEAD )	// this is a HoldNote begin marker.  Grade it, but don't draw
				continue;	// skip

			// See if there is a hold step that begins on this index.
			bool bHoldNoteBeginsOnThisBeat = false;
			for( int c2=0; c2<GetNumTracks(); c2++ )
			{
				if( GetTapNote(c2, i) == TAP_HOLD_HEAD )
				{
					bHoldNoteBeginsOnThisBeat = true;
					break;
				}
			}

			bool bIsInSelectionRange = false;
			if( m_fBeginMarker!=-1 && m_fEndMarker!=-1 )
			{
				float fBeat = NoteRowToBeat(i);
				bIsInSelectionRange = m_fBeginMarker<=fBeat && fBeat<=m_fEndMarker;
			}

			m_NoteDisplay[c].DrawTap( c, NoteRowToBeat(i), bHoldNoteBeginsOnThisBeat, bIsInSelectionRange ? fSelectedRangeGlow : m_fPercentFadeToFail );
		}
	}

}


void NoteField::RemoveTapNoteRow( int iIndex )
{
	for( int c=0; c<GetNumTracks(); c++ )
		SetTapNote(c, iIndex, TAP_EMPTY);
}

void NoteField::FadeToFail()
{
	m_fPercentFadeToFail = max( 0.0f, m_fPercentFadeToFail );	// this will slowly increase every Update()
		// don't fade all over again if this is called twice
}
