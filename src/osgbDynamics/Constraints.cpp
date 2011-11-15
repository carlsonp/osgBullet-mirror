/*************** <auto-copyright.pl BEGIN do not edit this line> **************
 *
 * osgBullet is (C) Copyright 2009-2011 by Kenneth Mark Bryden
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 *************** <auto-copyright.pl END do not edit this line> ***************/

#include <osgbDynamics/Constraints.h>
#include <osgbDynamics/MotionState.h>
#include <osgbCollision/Utils.h>

#include <osg/Object>
#include <osg/Notify>

#include <btBulletDynamicsCommon.h>

#include <osg/io_utils>


namespace osgbDynamics
{


Constraint::Constraint()
  : osg::Object(),
    _rbA( NULL ),
    _rbB( NULL ),
    _constraint( NULL ),
    _dirty( true )
{
}
Constraint::Constraint( btRigidBody* rbA, btRigidBody* rbB )
  : osg::Object(),
    _rbA( rbA ),
    _rbB( rbB ),
    _constraint( NULL ),
    _dirty( true )
{
}
Constraint::Constraint( btRigidBody* rbA, const osg::Matrix& rbAXform,
        btRigidBody* rbB, const osg::Matrix& rbBXform )
  : _rbA( rbA ),
    _rbB( rbB ),
    _rbAXform( rbAXform ),
    _rbBXform( rbBXform ),
    _constraint( NULL ),
    _dirty( true )
{
}
Constraint::Constraint( const Constraint& rhs, const osg::CopyOp& copyop )
  : osg::Object( rhs, copyop ),
    _rbA( rhs._rbA ),
    _rbB( rhs._rbB ),
    _rbAXform( rhs._rbAXform ),
    _rbBXform( rhs._rbBXform ),
    _constraint( rhs._constraint ),
    _dirty( rhs._dirty )
{
}
Constraint::~Constraint()
{
    // Deleting the constraint is up to the calling code. Something like this:
    //delete osgbDynamics::Constraint::getConstraint();
}

btTypedConstraint* Constraint::getConstraint() const
{
    if( getDirty() || ( _constraint == NULL ) )
    {
        Constraint* nonConst = const_cast< Constraint* >( this );
        nonConst->createConstraint();
    }

    return( _constraint );
}

void Constraint::setRigidBodies( btRigidBody* rbA, btRigidBody* rbB )
{
    _rbA = rbA;
    _rbB = rbB;
    setDirty();
}
void Constraint::setAXform( const osg::Matrix& rbAXform )
{
    _rbAXform = rbAXform;
    setDirty();
}
void Constraint::setBXform( const osg::Matrix& rbBXform )
{
    _rbBXform = rbBXform;
    setDirty();
}

bool Constraint::operator==( const Constraint& rhs ) const
{
    return( !( operator!=( rhs ) ) );
}
bool Constraint::operator!=( const Constraint& rhs ) const
{
    return(
        ( _rbAXform != rhs._rbAXform ) ||
        ( _rbBXform != rhs._rbBXform )
    );
}



SliderConstraint::SliderConstraint()
  : Constraint()
{
}
SliderConstraint::SliderConstraint( btRigidBody* rbA, btRigidBody* rbB )
  : Constraint( rbA, rbB )
{
    setDirty();
}
SliderConstraint::SliderConstraint( btRigidBody* rbA, const osg::Matrix& rbAXform,
        const osg::Vec3& slideAxisInA, const osg::Vec2& slideLimit )
  : Constraint( rbA, rbAXform ),
    _axis( slideAxisInA ),
    _slideLimit( slideLimit )
{
    setDirty();
}
SliderConstraint::SliderConstraint( btRigidBody* rbA, const osg::Matrix& rbAXform,
        btRigidBody* rbB, const osg::Matrix& rbBXform,
        const osg::Vec3& slideAxisInA, const osg::Vec2& slideLimit )
  : Constraint( rbA, rbAXform, rbB, rbBXform ),
    _axis( slideAxisInA ),
    _slideLimit( slideLimit )
{
    setDirty();
}
SliderConstraint::SliderConstraint( const SliderConstraint& rhs, const osg::CopyOp& copyop )
  : Constraint( rhs, copyop ),
    _axis( rhs._axis),
    _slideLimit( rhs._slideLimit )
{
}
SliderConstraint::~SliderConstraint()
{
    // Deleting the constraint is up to the calling code.
}

btSliderConstraint* SliderConstraint::getAsBtSlider() const
{
    return( static_cast< btSliderConstraint* >( getConstraint() ) );
}

void SliderConstraint::setAxis( const osg::Vec3& axis )
{
    _axis = axis;
    setDirty();
}
void SliderConstraint::setLimit( const osg::Vec2& limit )
{
    _slideLimit = limit;

    if( !getDirty() && ( _constraint != NULL ) )
    {
        // Dynamically modify the existing constraint.
        btSliderConstraint* sc = getAsBtSlider();
        sc->setLowerLinLimit( _slideLimit[ 0 ] );
        sc->setUpperLinLimit( _slideLimit[ 1 ] );
    }
    else
        setDirty();
}

bool SliderConstraint::operator==( const SliderConstraint& rhs ) const
{
    return( !( operator!=( rhs ) ) );
}
bool SliderConstraint::operator!=( const SliderConstraint& rhs ) const
{
    return(
        ( _axis != rhs._axis ) ||
        ( _slideLimit != rhs._slideLimit ) ||
        ( Constraint::operator!=( static_cast< const Constraint& >( rhs ) ) )
    );
}


void SliderConstraint::createConstraint()
{
    if( _rbA == NULL )
    {
        osg::notify( osg::INFO ) << "createConstraint: _rbA == NULL." << std::endl;
        return;
    }

    if( _constraint )
        delete _constraint;


    // Transform the world coordinate axis into A's local coordinates.
    osg::Matrix aOrient = _rbAXform;
    aOrient.setTrans( 0., 0., 0. );
    const osg::Vec3 axisInA = _axis * osg::Matrix::inverse( aOrient );


    const osg::Vec3 bulletSliderAxis( 1., 0., 0. );

    // Compute a matrix that transforms B's collision shape origin and x axis
    // to A's origin and slide axis.
    //   1. Matrix to align the (slider constraint) x axis with A's slide axis.
    const osg::Matrix axisRotate( osg::Matrix::rotate( bulletSliderAxis, axisInA ) );
    //
    //   2. Inverse B center of mass offset.
    osg::Vec3 bCom;
    if( _rbB != NULL )
    {
        osgbDynamics::MotionState* motion = dynamic_cast< osgbDynamics::MotionState* >( _rbB->getMotionState() );
        if( motion == NULL )
        {
            osg::notify( osg::WARN ) << "SliderConstraint: Invalid MotionState." << std::endl;
            return;
        }
        bCom = motion->getCenterOfMass();
    }
    const osg::Matrix invBCOM( osg::Matrix::translate( -( bCom ) ) );
    //
    //   3. Transform from B's origin to A's origin.
    const osg::Matrix rbBToRbA( osg::Matrix::inverse( _rbBXform ) * _rbAXform );
    //
    //   4. The final rbB frame matrix.
    btTransform rbBFrame = osgbCollision::asBtTransform(
        axisRotate * invBCOM * rbBToRbA );


    // Compute a matrix that transforms A's collision shape origin and x axis
    // to A's origin and drawerAxis.
    //   1. A's center of mass offset.
    osgbDynamics::MotionState* motion = dynamic_cast< osgbDynamics::MotionState* >( _rbA->getMotionState() );
    if( motion == NULL )
    {
        osg::notify( osg::WARN ) << "SliderConstraint: Invalid MotionState." << std::endl;
        return;
    }
    const osg::Matrix invACOM( osg::Matrix::translate( -( motion->getCenterOfMass() ) ) );
    //
    //   2. The final rbA frame matrix.
    btTransform rbAFrame = osgbCollision::asBtTransform(
        axisRotate * invACOM );


    btSliderConstraint* sc;
    if( _rbB != NULL )
        sc = new btSliderConstraint( *_rbA, *_rbB, rbAFrame, rbBFrame, false );
    else
        sc = new btSliderConstraint( *_rbA, rbAFrame, true );
    const btScalar loLimit = _slideLimit[ 0 ];
    const btScalar hiLimit = _slideLimit[ 1 ];
    sc->setLowerLinLimit( loLimit );
    sc->setUpperLinLimit( hiLimit );
    _constraint = sc;

    setDirty( false );
}



TwistSliderConstraint::TwistSliderConstraint()
  : SliderConstraint()
{
}
TwistSliderConstraint::TwistSliderConstraint( btRigidBody* rbA, btRigidBody* rbB )
  : SliderConstraint( rbA, rbB )
{
}
TwistSliderConstraint::TwistSliderConstraint( btRigidBody* rbA, const osg::Matrix& rbAXform,
            btRigidBody* rbB, const osg::Matrix& rbBXform,
            const osg::Vec3& slideAxisInA, const osg::Vec2& slideLimit )
  : SliderConstraint( rbA, rbAXform, rbB, rbBXform, slideAxisInA, slideLimit )
{
}
TwistSliderConstraint::TwistSliderConstraint( btRigidBody* rbA, const osg::Matrix& rbAXform,
            const osg::Vec3& slideAxisInA, const osg::Vec2& slideLimit )
  : SliderConstraint( rbA, rbAXform, slideAxisInA, slideLimit )
{
}
TwistSliderConstraint::TwistSliderConstraint( const TwistSliderConstraint& rhs, const osg::CopyOp& copyop )
  : SliderConstraint( rhs, copyop )
{
}
TwistSliderConstraint::~TwistSliderConstraint()
{
}

void TwistSliderConstraint::createConstraint()
{
    // Create the constraint using the base class.
    SliderConstraint::createConstraint();
    if( _constraint == NULL )
        return;

    // Base class produces a btSliderConstraint.
    btSliderConstraint* sc = getAsBtSlider();
    // All we need to do is disable the angular constraint that exists in
    // the btSliderConstraint by default.
    sc->setLowerAngLimit( -osg::PI );
    sc->setUpperAngLimit( osg::PI );

    setDirty( false );
}



LinearSpringConstraint::LinearSpringConstraint()
{
}
LinearSpringConstraint::~LinearSpringConstraint()
{
}

AngleSpringConstraint::AngleSpringConstraint()
{
}
AngleSpringConstraint::~AngleSpringConstraint()
{
}

LinearAngleSpringConstraint::LinearAngleSpringConstraint()
{
}
LinearAngleSpringConstraint::~LinearAngleSpringConstraint()
{
}

FixedConstraint::FixedConstraint()
{
}
FixedConstraint::~FixedConstraint()
{
}

PlanarConstraint::PlanarConstraint()
{
}
PlanarConstraint::~PlanarConstraint()
{
}

BoxConstraint::BoxConstraint()
{
}
BoxConstraint::~BoxConstraint()
{
}

HingeConstraint::HingeConstraint()
{
}
HingeConstraint::~HingeConstraint()
{
}

CardanConstraint::CardanConstraint()
{
}
CardanConstraint::~CardanConstraint()
{
}



BallAndSocketConstraint::BallAndSocketConstraint()
  : Constraint()
{
}
BallAndSocketConstraint::BallAndSocketConstraint( btRigidBody* rbA, btRigidBody* rbB )
  : Constraint( rbA, rbB )
{
}
BallAndSocketConstraint::BallAndSocketConstraint( btRigidBody* rbA, const osg::Matrix& rbAXform,
            btRigidBody* rbB, const osg::Matrix& rbBXform, const osg::Vec3& wcPoint )
  : Constraint( rbA, rbAXform, rbB, rbBXform ),
    _point( wcPoint )
{
}
BallAndSocketConstraint::BallAndSocketConstraint( btRigidBody* rbA, const osg::Matrix& rbAXform,
            const osg::Vec3& wcPoint )
  : Constraint( rbA, rbAXform ),
    _point( wcPoint )
{
}
BallAndSocketConstraint::BallAndSocketConstraint( const BallAndSocketConstraint& rhs, const osg::CopyOp& copyop )
  : Constraint( rhs, copyop ),
    _point( rhs._point )
{
}
BallAndSocketConstraint::~BallAndSocketConstraint()
{
}

btPoint2PointConstraint* BallAndSocketConstraint::getAsBtPoint2Point() const
{
    return( static_cast< btPoint2PointConstraint* >( getConstraint() ) );
}

void BallAndSocketConstraint::setPoint( const osg::Vec3& point )
{
    _point = point;
    setDirty();
}

bool BallAndSocketConstraint::operator==( const BallAndSocketConstraint& rhs ) const
{
    return( !( operator!=( rhs ) ) );
}
bool BallAndSocketConstraint::operator!=( const BallAndSocketConstraint& rhs ) const
{
    return(
        ( _point != rhs._point ) ||
        ( Constraint::operator!=( static_cast< const Constraint& >( rhs ) ) )
    );
}

void BallAndSocketConstraint::createConstraint()
{
    if( _rbA == NULL )
    {
        osg::notify( osg::INFO ) << "createConstraint: _rbA == NULL." << std::endl;
        return;
    }

    if( _constraint )
        delete _constraint;


    // Compute a matrix that transforms the world coord point into
    // A's collision shape local coordinates.
    //
    //   1. Handle A's center of mass offset.
    osg::Vec3 aCom;
    osgbDynamics::MotionState* motion = dynamic_cast< osgbDynamics::MotionState* >( _rbA->getMotionState() );
    if( motion == NULL )
    {
        osg::notify( osg::WARN ) << "BallAndSocketConstraint: Invalid MotionState." << std::endl;
        return;
    }
    aCom = motion->getCenterOfMass();
    //
    //   2. The final transform matrix.
    osg::Matrix rbAMatrix = osg::Matrix::inverse( osg::Matrix::translate( aCom ) * _rbAXform );

    // And now compute the WC point in rbA space:
    const btVector3 aPt = osgbCollision::asBtVector3( _point * rbAMatrix );


    // Compute a matrix that transforms the world coord point into
    // B's collision shape local coordinates.
    //
    //   1. Handle B's center of mass offset.
    osg::Vec3 bCom;
    if( _rbB != NULL )
    {
        motion = dynamic_cast< osgbDynamics::MotionState* >( _rbB->getMotionState() );
        if( motion == NULL )
        {
            osg::notify( osg::WARN ) << "BallAndSocketConstraint: Invalid MotionState." << std::endl;
            return;
        }
        bCom = motion->getCenterOfMass();
    }
    //
    //   2. The final transform matrix.
    const osg::Matrix rbBMatrix = osg::Matrix::inverse( osg::Matrix::translate( bCom ) * _rbBXform );

    // And now compute the WC point in rbB space:
    const btVector3 bPt = osgbCollision::asBtVector3( _point * rbBMatrix );


    btPoint2PointConstraint* cons;
    if( _rbB != NULL )
        cons = new btPoint2PointConstraint( *_rbA, *_rbB, aPt, bPt );
    else
        cons = new btPoint2PointConstraint( *_rbA, aPt );
    _constraint = cons;

    setDirty( false );
}


RagdollConstraint::RagdollConstraint()
{
}
RagdollConstraint::~RagdollConstraint()
{
}

WheelSuspensionConstraint::WheelSuspensionConstraint()
{
}
WheelSuspensionConstraint::~WheelSuspensionConstraint()
{
}


// osgbDynamics
}
