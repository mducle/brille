#include <random>
#include <chrono>
#include <catch.hpp>
#include <array>

#include "spg_database.h"
#include "linear_algebra.h"
#include "lattice.h"
#include "latvec.h"
#include "primitive_transforms.h"

TEST_CASE("Testing primitive transforms"){
  Centering c;
  SECTION("Body centering"){ c = BODY; }
  SECTION("Face centering"){ c = FACE; }
  SECTION("EndA centering"){ c = A_FACE; }
  SECTION("EndB centering"){ c = B_FACE; }
  SECTION("EndC centering"){ c = C_FACE; }
  SECTION("R centering"){ c = R_CENTER; }
  PrimitiveTransform PT(c);
  REQUIRE( !PT.does_nothing() );
  std::array<double,9> P = PT.get_to_primitive();
  std::array<int,9> invP = PT.get_from_primitive();
  double I[9]={1.,0.,0., 0.,1.,0., 0.,0.,1.};
  double res[9];
  multiply_matrix_matrix<double,int,double,3>(res,invP.data(),P.data());
  REQUIRE( approx_matrix<double,double,3>(res,I) );
}

TEST_CASE("Testing primitive vector transforms"){
  std::string spgr;
  SECTION("Primitive spacegroup"){
    spgr = "P 1";
  }
  SECTION("Body-centred spacegroup"){
    spgr = "Im-3m";
  }
  SECTION("Face-centred spacegroup"){
    spgr = "Fmm2";
  }
  Direct d(1,1,1,PI/2,PI/2,PI/2,spgr);
  Reciprocal r = d.star();

  std::default_random_engine generator(std::chrono::system_clock::now().time_since_epoch().count());
  std::uniform_real_distribution<double> distribution(-5.0,5.0);

  int nQ = 33;
  double* rawQ = new double[nQ*3]();
  for (int i=0; i<3*nQ; ++i) rawQ[i] = distribution(generator);
  LDVec<double> V(d,nQ,rawQ);
  LQVec<double> Q(r,nQ,rawQ);
  delete[] rawQ;

  // int nQ = 7;
  // double rawQ[] = {1.,0.,0., 0.,1.,0., 0.,0.,1., 1.,1.,0., 1.,0.,1., 0.,1.,1., 1.,1.,1.};
  // LDVec<double> V(d,nQ,rawQ);
  // LQVec<double> Q(r,nQ,rawQ);



  LDVec<double> Vp = transform_to_primitive(d,V);

  // Test 1: make sure that |Vᵢ|==|Vpᵢ| and |Qᵢ|==|Qpᵢ|
  for (int i=0; i<nQ; ++i)
    REQUIRE( Vp.norm(i) == Approx(V.norm(i)) );
  // Test 2: Check compoments, expressed in inverse Angstrom:
  ArrayVector<double> Vxyz = V.get_xyz();
  ArrayVector<double> Vpxyz = Vp.get_xyz();
  for (int i=0; i<nQ; ++i)
    REQUIRE( Vxyz.norm(i) == Approx(Vpxyz.norm(i)) );
  // // THE FOLLOWING WILL FAIL, since the xyz coordinate system is arbitrarily
  // // aligned with x along a, and the direction of a and a' are not guaranteed
  // // to be the same!
  // for (int i=0; i<nQ; ++i)
  //   for (int j=0; j<3; ++j)
  //     REQUIRE( Vpxyz.getvalue(i,j) == Approx(Vxyz.getvalue(i,j)) );
  // Test 3: check transfrom_from_primitive(transform_to_primitive(X)) == X
  LDVec<double> pVp = transform_from_primitive(d,Vp);
  // LQVec<double> pQp = transform_from_primitive(r,Qp);
  for (int i=0; i<nQ; ++i)
    for (int j=0; j<3; ++j)
      REQUIRE( pVp.getvalue(i,j) == Approx(V.getvalue(i,j)) );

  LQVec<double> Qp = transform_to_primitive(r,Q);

  // Test 1: make sure that |Qᵢ|==|Qpᵢ|
  for (int i=0; i<nQ; ++i)
    REQUIRE( Qp.norm(i) == Approx(Q.norm(i)) );
  // Test 2: Check compoments, expressed in inverse Angstrom:
  ArrayVector<double> Qxyz = Q.get_xyz();
  ArrayVector<double> Qpxyz = Qp.get_xyz();
  for (int i=0; i<nQ; ++i)
    REQUIRE( Qxyz.norm(i) == Approx(Qpxyz.norm(i)) );
  // // THE FOLLOWING WILL FAIL, since the xyz coordinate system is arbitrarily
  // // aligned with x along a, and the direction of a and a' are not guaranteed
  // // to be the same!
  // for (int i=0; i<nQ; ++i)
  //   for (int j=0; j<3; ++j)
  //     REQUIRE( Qpxyz.getvalue(i,j) == Approx(Qxyz.getvalue(i,j)) );
  // Test 3: check transfrom_from_primitive(transform_to_primitive(X)) == X
  LQVec<double> pQp = transform_from_primitive(r,Qp);
  for (int i=0; i<nQ; ++i)
    for (int j=0; j<3; ++j)
      REQUIRE( pQp.getvalue(i,j) == Approx(Q.getvalue(i,j)) );


}
