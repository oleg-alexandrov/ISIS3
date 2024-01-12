/** This is free and unencumbered software released into the public domain.
The authors of ISIS do not claim copyright on the contents of this file.
For more details about the LICENSE terms and the AUTHORS, you will
find files of those names at the top level of this repository. **/

/* SPDX-License-Identifier: CC0-1.0 */
#include "DemShape.h"

// Qt third party includes
#include <QDebug>
#include <QVector>

// c standard library third party includes
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <iomanip>
#include <string>
#include <vector>

// naif third party includes
#include <SpiceUsr.h>
#include <SpiceZfc.h>
#include <SpiceZmc.h>

#include "Cube.h"
#include "CubeManager.h"
#include "Distance.h"
#include "EllipsoidShape.h"
//#include "Geometry3D.h"
#include "IException.h"
#include "Interpolator.h"
#include "Latitude.h"
//#include "LinearAlgebra.h"
#include "Longitude.h"
#include "NaifStatus.h"
#include "Portal.h"
#include "Projection.h"
#include "Pvl.h"
#include "Spice.h"
#include "SurfacePoint.h"
#include "Table.h"
#include "Target.h"
#include "UniqueIOCachingAlgorithm.h"

using namespace std;

namespace Isis {
  /**
   * Construct a DemShape object. This method creates a ShapeModel object named
   * "DemShape". The member variables are set to Null.
   *
   */
  DemShape::DemShape() : ShapeModel() {
    setName("DemShape");
    m_demProj = NULL;
    m_demCube = NULL;
    m_interp = NULL;
    m_portal = NULL;
    m_demValueFound = false;
    m_demValue = -std::numeric_limits<double>::max();
  }


  /**
   * Construct a DemShape object. This method creates a ShapeModel object
   * named "DemShape" and initializes member variables from the projection
   * shape model using the given Target and Pvl.
   *
   * @param target Pointer to a valid target.
   * @param pvl Valid ISIS cube label.
   */
  DemShape::DemShape(Target *target, Pvl &pvl) : ShapeModel(target) {
    setName("DemShape");
    m_demProj = NULL;
    m_demCube = NULL;
    m_interp = NULL;
    m_portal = NULL;
    m_demValueFound = false;
    m_demValue = -std::numeric_limits<double>::max();

    PvlGroup &kernels = pvl.findGroup("Kernels", Pvl::Traverse);

    QString demCubeFile;
    if (kernels.hasKeyword("ElevationModel")) {
      demCubeFile = (QString) kernels["ElevationModel"];
    }
    else if(kernels.hasKeyword("ShapeModel")) {
      demCubeFile = (QString) kernels["ShapeModel"];
    }

    m_demCube = CubeManager::Open(demCubeFile);

    // This caching algorithm works much better for DEMs than the default,
    //   regional. This is because the uniqueIOCachingAlgorithm keeps track
    //   of a history, which for something that isn't linearly processing a
    //   cube is worth it. The regional caching algorithm tosses out results
    //   from iteration 1 of setlookdirection (first algorithm) at iteration
    //   4 and the next setimage has to re-read the data.
    m_demCube->addCachingAlgorithm(new UniqueIOCachingAlgorithm(5));
    m_demProj = m_demCube->projection();
    m_interp = new Interpolator(Interpolator::BiLinearType);
    m_portal = new Portal(m_interp->Samples(), m_interp->Lines(),
                            m_demCube->pixelType(),
                            m_interp->HotSample(), m_interp->HotLine());

    // Read in the Scale of the DEM file in pixels/degree
    const PvlGroup &mapgrp = m_demCube->label()->findGroup("Mapping", Pvl::Traverse);

    // Save map scale in pixels per degree
    m_pixPerDegree = (double) mapgrp["Scale"];
  }


  //! Destroys the DemShape
  DemShape::~DemShape() {
    m_demProj = NULL;

    // We do not have ownership of p_demCube
    m_demCube = NULL;

    delete m_interp;
    m_interp = NULL;

    delete m_portal;
    m_portal = NULL;
  }

  /**
     Given a position along a ray, compute the difference between the 
     radius at that position and the surface radius at that lon-lat location.
     All lengths are in km.
   * @param observerPos Observer position
   * @param lookDirection Look direction
   * @param t parameter measuring location on the ray
   * @param intersectionPoint location along the ray, eventual intersection location
   * @param success True if the calculation was successful
   * @return @d double Signed error, if the calculation was successful
   **/
  double DemShape::demError(vector<double> const& observerPos,
                            vector<double> const& lookDirection, 
                            double t, 
                            double * intersectionPoint,
                            bool & success) {
  
    // Initialize the return value
    success = false;
    
    // Compute the position along the ray
    for (size_t i = 0; i < 3; i++)
      intersectionPoint[i] = observerPos[i] + t * lookDirection[i];

    double pointRadiusKm = sqrt(intersectionPoint[0]*intersectionPoint[0] +
                               intersectionPoint[1]*intersectionPoint[1] +
                               intersectionPoint[2]*intersectionPoint[2]);
          
    // The lat/lon calculations are done here by hand for speed & efficiency
    // With doing it in the SurfacePoint class using p_surfacePoint, there
    // is a 24% slowdown (which is significant in this very tightly looped call).
    double norm2 = intersectionPoint[0] * intersectionPoint[0] +
        intersectionPoint[1] * intersectionPoint[1];
    double latDD = atan2(intersectionPoint[2], sqrt(norm2)) * RAD2DEG;
    double lonDD = atan2(intersectionPoint[1], intersectionPoint[0]) * RAD2DEG;
    if (lonDD < 0) {
      lonDD += 360;
    }
    
    std::cout << "--new lat and lon is " << latDD << " " << lonDD << std::endl;
    
    // Previous Sensor version used local version of this method with lat and lon doubles.
    // Steven made the change to improve speed.  He said the difference was negligible.
    Distance surfaceRadiusKm = localRadius(Latitude(latDD, Angle::Degrees),
                                           Longitude(lonDD, Angle::Degrees));
    
    if (Isis::IsSpecial(surfaceRadiusKm.kilometers())) {
      std::cout << "--failed here" << std::endl;
      setHasIntersection(false);
      success = false;
      return -1; // return something
    }
    
    // Must set these to be able to compute resolution later
    surfaceIntersection()->FromNaifArray(intersectionPoint);
    setHasIntersection(true);
    
    std::cout << "--t, surface rad, pt rad, and diff " 
              << t << " "
              << surfaceRadiusKm.kilometers() << " " 
              << pointRadiusKm << " " 
              << pointRadiusKm - surfaceRadiusKm.kilometers() 
              << std::endl;
              
    success = true;
    return pointRadiusKm - surfaceRadiusKm.kilometers();
  } 
  
  /**
   * Find the intersection point with the DEM. Start by intersecting
   * with a nearby horizontal surface, then refine using the secant method.
   * This was validated to work with ground-level sensors. Likely can
     do well with images containing a limb.

   TODO(oalexan1): Must estimate the mean elevation outside this function!
   *
   * @param observerPos
   * @param lookDirection
   *
   * @return @b bool Indicates whether the intersection was found.
   */
  bool DemShape::intersectSurface(vector<double> observerPos,
                                  vector<double> lookDirection) {
  
    // try to intersect the target body ellipsoid as a first approximation
    // for the iterative DEM intersection method
    // (this method is in the ShapeModel base class)
    std::cout.precision(17);
    std::cout << "--xnow in intersectSurface" << std::endl;
    // print observerPos and lookDirection
    
    // Find norm of observerPos
    double positionNormKm = 0.0;
    for (size_t i = 0; i < observerPos.size(); i++)
      positionNormKm += observerPos[i]*observerPos[i];
    positionNormKm = sqrt(positionNormKm);
    std::cout << "\nnorm is " << positionNormKm << std::endl;

    // in each iteration, the current surface intersect point is saved for
    // comparison with the new, updated surface intersect point
    SpiceDouble currentIntersectPt[3];
    SpiceDouble newIntersectPt[3];

    // std::cout << "---faking the look direction" << std::endl;
    //std::cout << "look direction is " << lookDirection[0] << " " << lookDirection[1] << " " << lookDirection[2] << std::endl;
    
    std::cout << "--look dot with down direction is " << -(lookDirection[0]*observerPos[0] + lookDirection[1]*observerPos[1] + lookDirection[2]*observerPos[2]) / positionNormKm << std::endl;
    
    // An estimate for the radius of points in the DEM. Ensure the radius is
    // strictly below the position, so that surfpt_c does not fail.
    double r = findDemValue();
    r = std::min(r, positionNormKm - 0.0001);
    std::cout << "demRadiusKm is " << r << std::endl;
    
    std::cout << "---xr is " << r << std::endl;
    bool status;
    surfpt_c((SpiceDouble *) &observerPos[0], &lookDirection[0], r, r, r, newIntersectPt,
               (SpiceBoolean*) &status);
  
    std::cout << "--new intersect point is " << newIntersectPt[0] << " " << newIntersectPt[1] << " " << newIntersectPt[2] << std::endl;
    std::cout << "--new status is " << status << std::endl;
    
    if (!status) {  
      std::cout << "--failed at ellipse intersected" << std::endl;
      return false;
    }
    
    surfaceIntersection()->FromNaifArray(newIntersectPt);
    setHasIntersection(true);
    
    std::cout << "resolution is " << resolution() << " m/pix" << std::endl;

    // Before calling resolution(), must ensure the intersection point is set 
    double tol = resolution()/100;  // 1/100 of a pixel
    std::cout << "--tol is " << tol << std::endl;
    
    // Will use secant method
    // Find the current position along the ray, relative to the observer
    // Equation: newIntersectPt = observerPos + t * lookDirection
    double t0 = ((newIntersectPt[0] - observerPos[0]) * lookDirection[0] +
                 (newIntersectPt[1] - observerPos[1]) * lookDirection[1] +
                 (newIntersectPt[2] - observerPos[2]) * lookDirection[2]) 
                 / (lookDirection[0] * lookDirection[0] +
                    lookDirection[1] * lookDirection[1] +
                    lookDirection[2] * lookDirection[2]);
                 
    bool success = false;
    double intersectionPoint[3];
    
    // Initial guess
    double f0 = demError(observerPos, lookDirection, t0, intersectionPoint, success); 
    std::cout << "t0, f0, success is " << t0 << " " << f0 << " " << success << std::endl;
    if (!success) {
      std::cout << "--failed at ellipse intersected" << std::endl;
      return false;
    }
    
    // Add 0.1 meters to get the next guess
    double t1 = t0 + 0.0001;
    double f1 = demError(observerPos, lookDirection, t1, intersectionPoint, success);
    std::cout << "t1, f1, success is " << t1 << " " << f1 << " " << success << std::endl;
    if (!success) {
      std::cout << "--failed at ellipse intersected" << std::endl;
      return false;
    }

    // Do secant method with at most 100 iterations
    bool converged = false;
    for (int i = 1; i <= 100; i++) {
      
      std::cout << "--sec it is " << i << std::endl;
      std::cout << "--t0, f0, t1, f1 is " << t0 << " " << f0 << " " << t1 << " " << f1 << std::endl;
      std::cout << "ttt_secant it err_m " << i << " " << std::abs(f1) * 1000.0 << " meters" << std::endl;
      
      // Now recompute tolerance at updated surface point and recheck
      if (std::abs(f1) * 1000.0 < tol) {
        
        surfaceIntersection()->FromNaifArray(intersectionPoint);
        std::cout << "--recomputing resolution as " << resolution() << " m/pix " << std::endl;
        tol = resolution() / 100.0;
        std::cout << "--recomputing tol as " << tol << " meters" << std::endl;

        if (std::abs(f1) * 1000.0 < tol) {
          std::cout << "--converged" << std::endl;
          converged = true;
          setHasIntersection(true);
          break;
        }
      }
      
      // If the function values are large but are equal,
      // there is nothing we can do
      if (f1 == f0 && std::abs(f1) * 1000.0 >= tol) {
        converged = false;
        break;
      }
      
      // Secant method iteration
      double t2 = t1 - f1 * (t1 - t0) / (f1 - f0);
      double f2 = demError(observerPos, lookDirection, t2, intersectionPoint, success);
      std::cout << "--t2, f2, success is " << t2 << " " << f2 << " " << success << std::endl;
      
      if (!success) {
        converged = false;
        break;
      }
      
      // Update
      t0 = t1; f0 = f1;
      t1 = t2; f1 = f2;
    }

    NaifStatus::CheckErrors();
    
    // TODO(oalexan1): fix here!
    //return converged;
        
    static const int maxit = 100;
    int it = 1;
    double dX, dY, dZ, dist2;
    bool done = false;

    // latitude, longitude in Decimal Degrees
    double latDD, lonDD;

    double tol2 = tol * tol;

    NaifStatus::CheckErrors();
    while (!done) {
      
      std::cout << "\n--old it is " << it << std::endl;
      std::cout << "-- tol " << tol << " meters" << std::endl;

      if (it > maxit) {
        std::cout << "--failed in iter" << std::endl;
        setHasIntersection(false);
        done = true;
        continue;
      }

      // The lat/lon calculations are done here by hand for speed & efficiency
      // With doing it in the SurfacePoint class using p_surfacePoint, there
      // is a 24% slowdown (which is significant in this very tightly looped call).
      double norm2 = newIntersectPt[0] * newIntersectPt[0] +
          newIntersectPt[1] * newIntersectPt[1];
      // TODO(oalexan1): Find local radius at DEM center!
      // Use that as initial guess above!
      latDD = atan2(newIntersectPt[2], sqrt(norm2)) * RAD2DEG;
      lonDD = atan2(newIntersectPt[1], newIntersectPt[0]) * RAD2DEG;

      if (lonDD < 0) {
        lonDD += 360;
      }
      
      std::cout << "--new lat and lon is " << latDD << " " << lonDD << std::endl;
      
      // if (latDD > minLat && latDD < maxLat) 
      //   std::cout << "--lat is in the box" << std::endl;
      // else 
      //   std::cout << "--lat is not in the box" << std::endl;
      // if (lonDD > minLon && lonDD < maxLon) 
      //   std::cout << "--lon is in the box" << std::endl;
      // else 
      //   std::cout << "--lon is not in the box" << std::endl;
      // std::cout << "--lat are " << minLat << " " << latDD << " " << maxLat << std::endl;
      // std::cout << "--lon are " << minLon << " " << lonDD << " " << maxLon << std::endl;
      
      // Previous Sensor version used local version of this method with lat and lon doubles.
      // Steven made the change to improve speed.  He said the difference was negligible.
      Distance radiusKm = localRadius(Latitude(latDD, Angle::Degrees),
                                      Longitude(lonDD, Angle::Degrees));
      
      if (Isis::IsSpecial(radiusKm.kilometers())) {
        std::cout << "--failed here" << std::endl;
        setHasIntersection(false);
        exit (1);
        return false;
      }

      // save current surface intersect point for comparison with new, updated
      // surface intersect point
      memcpy(currentIntersectPt, newIntersectPt, 3 * sizeof(double));
      // TODO(oalexan1): Why below use same radius in x, y, z?
      double r = radiusKm.kilometers();

      std::cout << "--new r is " << r*1000 << " meters" << std::endl;
      std::cout << "--new status is " << status << std::endl;

      bool status;
      surfpt_c((SpiceDouble *) &observerPos[0], &lookDirection[0], r, r, r, newIntersectPt,
               (SpiceBoolean*) &status);
      
      // LinearAlgebra::Vector point = LinearAlgebra::vector(observerPos[0],
      //                                                     observerPos[1],
      //                                                     observerPos[2]);
      // LinearAlgebra::Vector direction = LinearAlgebra::vector(lookDirection[0],
      //                                                         lookDirection[1],
      //                                                         lookDirection[2]);
      // QList<double> ellipsoidRadii;
      // ellipsoidRadii << r << r << r;
      // LinearAlgebra::Vector newPt = Geometry3D::intersect(point, direction, ellipsoidRadii);

      setHasIntersection(status);
      if (!status) {
        std::cout << "--failed here2" << std::endl;
        exit(1);
        return status;
      }
      std::cout << "--curr intersect point  is " << 1000*currentIntersectPt[0] << " " << 1000*currentIntersectPt[1] << " " << 1000*currentIntersectPt[2] << " meters" << std::endl;
      std::cout << "--new intersect point is " << 1000*newIntersectPt[0] << " " << 1000*newIntersectPt[1] << " " << 1000*newIntersectPt[2] << " meters" << std::endl;

      dX = currentIntersectPt[0] - newIntersectPt[0];
      dY = currentIntersectPt[1] - newIntersectPt[1];
      dZ = currentIntersectPt[2] - newIntersectPt[2];
      std::cout << "--diff is " << 1000* dX << " " << 1000*dY << " " << 1000*dZ << " meters" << std::endl;
      std::cout << "--tol is " << tol << " meters" << std::endl;


      dist2 = (dX*dX + dY*dY + dZ*dZ) * 1000 * 1000;
      std::cout << "ttt_fixed it err_m " << it << " " << sqrt(dist2) << " meters\n";
      std::cout << "err is " << sqrt(dist2) << " meters" << std::endl;

      // Now recompute tolerance at updated surface point and recheck
      if (dist2 < tol2) {
        surfaceIntersection()->FromNaifArray(newIntersectPt);
        std::cout << "--recomputing resolution as " << resolution() << " m/pix " << std::endl;
        tol = resolution() / 100.0;
        std::cout << "--recomputing tol as " << tol << " meters" << std::endl;
        tol2 = tol * tol;
        if (dist2 < tol2) {
          setHasIntersection(true);
          done = true;
        }
      }

      it++;
      
      std::cout << "--zzz10 tol " << tol << " meters" << std::endl;
    } // end of while loop
    
    NaifStatus::CheckErrors();

    std::cout << "--final has intersection is " << hasIntersection() << std::endl;
    //exit(0);
    
    return hasIntersection();
  }


  /**
   * Find a value in the DEM. Used when intersecting a ray with the DEM. 
   * Returned value is in km.
   */
  double DemShape::findDemValue() {
    
    if (m_demValueFound) 
      return m_demValue;
    
    int numSamples = m_demCube->sampleCount();
    int numLines = m_demCube->lineCount();
    
    // Try to pick about 25 samples not too close to the boundary. Stop at the
    // first successful one.
    int num = 5;
    int sampleSpacing = std::max(numSamples / (num + 1), 1);
    int lineSpacing = std::max(numLines / (num + 1), 1);

    // iterate as sample from sampleSpacing to numSamples-sampleSpacing
    for (int s = sampleSpacing; s <= numSamples - sampleSpacing; s += sampleSpacing) {
      for (int l = lineSpacing; l <= numLines - lineSpacing; l += lineSpacing) {

        m_portal->SetPosition(s, l, 1);
        m_demCube->read(*m_portal);
        if (!Isis::IsSpecial(m_portal->DoubleBuffer()[0])) {
          m_demValue = m_portal->DoubleBuffer()[0] / 1000.0;
          m_demValueFound = true;
          return m_demValue;
        }
      }
    }
    
    // If no luck, return the mean radius of the target
    vector<Distance> radii = targetRadii();
    double a = radii[0].kilometers();
    double b = radii[1].kilometers();
    double c = radii[2].kilometers();
    
    m_demValue = (a + b + c)/3.0;
  
    m_demValueFound = true;  
    return m_demValue;
  }
  
  /**
   * Gets the radius from the DEM, if we have one.
   *
   * @param lat Latitude
   * @param lon Longitude
   *
   * @return @b Distance Local radius from the DEM
   */
  Distance DemShape::localRadius(const Latitude &lat, const Longitude &lon) {

    Distance distance=Distance();

    if (lat.isValid() && lon.isValid()) {
      m_demProj->SetUniversalGround(lat.degrees(), lon.degrees());

      // The next if statement attempts to do the same as the previous one, but not as well so
      // it was replaced.
      // if (!m_demProj->IsGood())
      //   return Distance();
      std::cout << "--set position to " << m_demProj->WorldX() << " " << m_demProj->WorldY() << std::endl;

      m_portal->SetPosition(m_demProj->WorldX(), m_demProj->WorldY(), 1);

      m_demCube->read(*m_portal);
      std::cout << "--ppp value is " << m_portal->DoubleBuffer()[0] << std::endl;

      distance = Distance(m_interp->Interpolate(m_demProj->WorldX(),
                                                m_demProj->WorldY(),
                                                m_portal->DoubleBuffer()),
                                                Distance::Meters);
    }

    return distance;
  }


  /**
   * Return the scale of the DEM shape, in pixels per degree.
   *
   * @return @b double The scale of the DEM.
   */
  double DemShape::demScale() {
    return m_pixPerDegree;
  }


  /**
   * This method calculates the default normal (Ellipsoid for backwards
   * compatibility) for the DemShape.
   */

  void DemShape::calculateDefaultNormal() {

    if (!surfaceIntersection()->Valid() || !hasIntersection() ) {
      IString msg = "A valid intersection must be defined before computing the surface normal";
      throw IException(IException::Programmer, msg, _FILEINFO_);
    }

    // Get the coordinates of the current surface point
    SpiceDouble pB[3];
    pB[0] = surfaceIntersection()->GetX().kilometers();
    pB[1] = surfaceIntersection()->GetY().kilometers();
    pB[2] = surfaceIntersection()->GetZ().kilometers();

    // Get the radii of the ellipsoid
    vector<Distance> radii = targetRadii();
    double a = radii[0].kilometers();
    double b = radii[1].kilometers();
    double c = radii[2].kilometers();

    vector<double> normal(3,0.);

    NaifStatus::CheckErrors();
    surfnm_c(a, b, c, pB, (SpiceDouble *) &normal[0]);
    NaifStatus::CheckErrors();

    setNormal(normal);
    setHasNormal(true);

  }



  /**
   * Returns the DEM Cube object.
   *
   * @return @b Cube* A pointer to the DEM cube associated with this shape model.
   */
  Cube *DemShape::demCube() {
    return m_demCube;
  }


  /**
   * Indicates that this shape model is from a DEM. Since this method returns
   * true for this class, the Camera class will calculate the local normal
   * using neighbor points. This method is pure virtual and must be
   * implemented by all DemShape classes. This parent implementation returns
   * true.
   *
   * @return @b bool Indicates that this is a DEM shape model.
   */
  bool DemShape::isDEM() const {
    return true;
  }


  /**
   * This method calculates the local surface normal of the current intersection
   * point.
   *
   * @param neighborPoints
   */
  void DemShape::calculateLocalNormal(QVector<double *> neighborPoints) {

    std::vector<SpiceDouble> normal(3);
    if (neighborPoints.isEmpty()) {
      normal[0] = normal[1] = normal[2] = 0.0;
      setNormal(normal);
      setHasNormal(false);
      return;
    }

    // subtract bottom from top and left from right and store results
    double topMinusBottom[3];
    vsub_c(neighborPoints[0], neighborPoints[1], topMinusBottom);
    double rightMinusLeft[3];
    vsub_c(neighborPoints[3], neighborPoints [2], rightMinusLeft);

    // take cross product of subtraction results to get normal
    ucrss_c(topMinusBottom, rightMinusLeft, (SpiceDouble *) &normal[0]);

    // unitize normal (and do sanity check for magnitude)
    double mag;
    unorm_c((SpiceDouble *) &normal[0], (SpiceDouble *) &normal[0], &mag);

    if (mag == 0.0) {
      normal[0] = normal[1] = normal[2] = 0.0;
      setNormal(normal);
      setHasNormal(false);
      return;
   }
    else {
      setHasNormal(true);
    }

    // Check to make sure that the normal is pointing outward from the planet
    // surface. This is done by taking the dot product of the normal and
    // any one of the unitized xyz vectors. If the normal is pointing inward,
    // then negate it.
    double centerLookVect[3];
    SpiceDouble pB[3];
    surfaceIntersection()->ToNaifArray(pB);
    unorm_c(pB, centerLookVect, &mag);
    double dotprod = vdot_c((SpiceDouble *) &normal[0], centerLookVect);
    if (dotprod < 0.0) {
      vminus_c((SpiceDouble *) &normal[0], (SpiceDouble *) &normal[0]);
    }

    setNormal(normal);
  }


  /**
   * This method calculates the surface normal of the current intersection
   * point.
   */
  void DemShape::calculateSurfaceNormal() {
    calculateDefaultNormal();
  }


}
