#include "safealloc.h"
#define ZERO_PREC 1e-10
////////////////////////////
// ALL NEW METHODS !!!!!! //
////////////////////////////
//using namespace rsm;

// trace of a square matrix
template<typename T, int N> T trace(const T *M){
	T out = T(0);
	for (int i=0; i<N; i++) out += M[i*(1+N)];
    return out;
}

// copying array from source (S) to destination (D)
template<typename T, int N, int M> void copy_array(T *D, const T *S){ for (int i=0; i<N*M; i++) D[i]=S[i]; }
template<typename T, int N> void copy_matrix(T *M, const T *A){ copy_array<T,N,N>(M,A); }
template<typename T, int N> void copy_vector(T *V, const T *A){ copy_array<T,N,1>(V,A); }

// absolute value of a scalar
template<typename T> T my_abs(const T a){ return (a<0 ? T(-1)*a : a); }

// element-wise checking of approximate equality
template<typename T, int N, int M> bool equal_array(const T *A, const T *B, const T tol){
	for (int i=0; i<N*M; i++) if ( my_abs(A[i]-B[i]) > tol ) return false;
	return true;
}
template<typename T, int N> bool equal_matrix(const T *A, const T *B, const T tol){ return equal_array<T,N,N>(A,B,tol); }
template<typename T, int N> bool equal_vector(const T *A, const T *B, const T tol){ return equal_array<T,N,1>(A,B,tol); }

template<typename T, typename R> bool approx_scalar(const T a, const R b){
	bool isfpT, isfpR;
	isfpT = std::is_floating_point<T>::value;
	isfpR = std::is_floating_point<R>::value;
	T Ttol = std::numeric_limits<T>::epsilon(); // zero for integer-type T
	R Rtol = std::numeric_limits<R>::epsilon(); // zero for integer-type R
	bool useTtol = false;
	if ( isfpT || isfpR ){
		if (isfpT && isfpR){
			if (std::is_convertible<R,T>::value) useTtol=true;
			else if (!std::is_convertible<T,R>::value) return false; // they can't be equal in this case
		} else if ( isfpT ) useTtol=true;
	}
	// if both a and b are close to epsilon for its type, our comparison of |a-b| to |a+b| might fail
	bool answer;
	if ( my_abs(a) <= 100*Ttol && my_abs(b) <= 100*Rtol )
		answer = my_abs(a-b) < 100*(useTtol ? Ttol :Rtol);
	else
		answer = my_abs(a-b) < 100*(useTtol ? Ttol :Rtol)*my_abs(a+b);
	return answer;
	// return ( my_abs(a-b) > 100*(useTtol ? Ttol :Rtol)*my_abs(a+b) ) ? false : true;
}
template<typename T, typename R, int N, int M> bool approx_array(const T *A, const R *B){
	bool isfpT, isfpR;
	isfpT = std::is_floating_point<T>::value;
	isfpR = std::is_floating_point<R>::value;
	T Ttol = std::numeric_limits<T>::epsilon(); // zero for integer-type T
	R Rtol = std::numeric_limits<R>::epsilon(); // zero for integer-type R
	bool useTtol = false;
	if ( isfpT || isfpR ){
		if (isfpT && isfpR){
			if (std::is_convertible<R,T>::value) useTtol=true;
			else if (!std::is_convertible<T,R>::value) return false; // they can't be equal in this case
		} else if ( isfpT ) useTtol=true;
	}
	// tol is defined for any combinations of types (might be zero).
	// since tol is epsilon, we need to make sure the value we check is scaled
	// by the sum of the items we're looking at the difference of
	for (int i=0; i<N*M; i++) if ( my_abs(A[i]-B[i]) > (useTtol ? Ttol : Rtol)*my_abs(A[i]+B[i]) ) return false;
	return true;
}
template<typename T, typename R, int N=3> bool approx_matrix(const T *A, const R *B){return approx_array<T,R,N,N>(A,B);}
template<typename T, typename R, int N=3> bool approx_vector(const T *A, const R *B){return approx_array<T,R,N,1>(A,B);}


// array multiplication C = A * B -- where C is (N,M), A is (N,I) and B is (I,M)
template<typename T, typename R, typename S, int N, int I, int M> void multiply_arrays(T *C, const R *A, const S *B){
	for (int i=0;i<N*M;i++) C[i]=T(0);
	// for (int i=0;i<N;i++) for (int j=0;j<M;j++) for (int k=0;k<I;k++) C[i+j*N] += T(A[i+k*N]*B[k+j*I]);
	for (int i=0;i<N;i++) for (int j=0;j<M;j++) for (int k=0;k<I;k++) C[i*M+j] += T(A[i*I+k]*B[k*M+j]);
}
template<typename T, typename R, typename S, int N> void multiply_matrix_matrix(T *C, const R *A, const S *B){ multiply_arrays<T,R,S,N,N,N>(C,A,B); }
template<typename T, typename R, typename S, int N> void multiply_matrix_vector(T *C, const R *A, const S *b){ multiply_arrays<T,R,S,N,N,1>(C,A,b); }
template<typename T, typename R, typename S, int N> void multiply_vector_matrix(T *C, const R *a, const S *B){ multiply_arrays<T,R,S,1,N,N>(C,a,B); }

// array element-wise addition
template<typename T, typename R, typename S, int N, int M> void add_arrays(T *C, const R *A, const S *B){ for (int i=0; i<N*M; i++) C[i] = T(A[i]+B[i]); }
template<typename T, typename R, typename S, int N> void add_matrix(T *C, const R *A, const S *B){ add_arrays<T,R,S,N,N>(C,A,B); }

// element-wise re-(special)-casting of arrays
template<typename T, typename R, int N, int M> void cast_array(T *A, const R *B){;}
template<typename R, int N, int M> void cast_array(int    *A, const R *B){ for (int i=0; i<N*M; i++) A[i] = cast_to_int(B[i]); }
template<typename R, int N, int M> void cast_array(double *A, const R *B){ for (int i=0; i<N*M; i++) A[i] = cast_to_dbl(B[i]); }

template<typename T, typename R, int N> void cast_matrix(T *A, const R *B){ cast_array<T,R,N,N>(A,B); }
template<typename T, typename R, int N> void cast_vector(T *A, const R *B){ cast_array<T,R,N,1>(A,B); }


// The cofactor array C_ij of the N by M array A is a M-1 by N-1 array of transpose(A with row i and column j) missing:
template<typename R> void array_cofactor(R *C, const R *A, const int i, const int j, const int N, const int M){
	int k = 0;
	for (int m=0; m<M; m++) for (int n=0; n<N; n++) if (i!=n && j!=m) C[k++] = A[n+N*m];
}
template<typename R> void matrix_cofactor(R *C, const R *A, const int i, const int j, const int N){ array_cofactor<R>(C,A,i,j,N,N); }

// the determinant is only defined for square matrices
template<typename R> R matrix_determinant(const R *M, const int N){
	if (1==N) return M[0];
	// the determinant:
	R d = 0.0;
	// temporary cofactor storage
	R *cof = safealloc<R>( (N-1)*(N-1) );

	// loop over one row/column (let's go with row, for fun)
	R pm = 1; // +/-1 for alternating elements
	for (int i=0; i<N; i++){
		matrix_cofactor(cof,M,0,i,N);
		d += pm * M[i*N] * matrix_determinant(cof,N-1);
		pm *= -1;
	}
	delete[] cof;
	return d;
}

// the adjoint is also only defined for a square matrix
template<typename R> void matrix_adjoint(R *A, const R *M, const int N){
	if (1==N){
		A[0]=R(1);
		return;
	}
	R pm=1, *cof = safealloc<R>((N-1)*(N-1));
	for (int i=0; i<N; i++) for (int j=0; j<N; j++){
			// the cofactor matrix for M[i,j]
			matrix_cofactor(cof,M,i,j,N);
			// if i+j is even, multiply the determinant by +1, otherwise -1
			pm = ( (i+j)%2==0 ) ? R(1) : R(-1);
			// We want to save the transpose, so A[j+i*n]
			A[j+i*N] = pm * matrix_determinant(cof,N-1);
	}
	delete[] cof;
}

// the inverse is, yet again, only defined for square matrices (with non-zero determinants)
template<typename R> R matrix_determinant_and_inverse(R *invM, const R *M, const R tol, const int N){
	R d = matrix_determinant(M,N);
	if (my_abs(d) > tol ){
		// the matrix is *not* singular and has an inverse
		matrix_adjoint(invM,M,N); // at this point invM is the Adjoint(M)
		// The inv(M) = adjoint(M)/det(M)
		int nn = N*N;
		for (int i=0; i<nn; i++) invM[i] /= d;
		// now invM is inv(M)!
	}
	return d;
}
template<typename R> bool matrix_inverse(R *invM, const R *M, R tol, const int N){ return ( my_abs( matrix_determinant_and_inverse(invM, M, tol, N) ) > tol) ; }

template<typename T, int N> bool similar_matrix(T *M, const T *A, const T *B, const T tol){
	T *C = safealloc<T>(N*N);
	bool ok = matrix_inverse(C,B,tol,N);
	if ( ok ){
		multiply_matrix_matrix<T,T,T,N>(M,A,B);
		multiply_matrix_matrix<T,T,T,N>(M,C,M);
	} else {
		printf("spglib: No similar matrix due to 0 determinant.\n");
	}
	delete[] C;
	return ok;
}

template<typename R, int N, int M> void array_transpose(R *D, const R *S){ for (int i=0; i<M; i++) for (int j=0; i<N; j++) D[i+j*M] = S[j+i*N]; }
template<typename R, int N> void matrix_transpose(R *D, const R *S){ array_transpose<R,N,N>(D,S); }
// in place transpose:
template<typename R, int N> void matrix_transpose(R *B){
	R t;
	for (int i=0; i<N; i++) for(int j=i+1; j<N; j++) {
		t = B[i+j*3];
		B[i+j*3] = B[j+i*3];
		B[j+i*3] = t;
	}
}

template<typename R, int N> void matrix_metric(R *M, const R *L){
	R *Lt = safealloc<R>(N*N);
	matrix_transpose<R,N>(Lt,L);
	multiply_matrix_matrix<R,N>(M,Lt,L);
	delete[] Lt;
}

template<typename R, int N> R vector_norm_squared(const R *v){
	R vv = 0;
	for (int i=0; i<N; i++) vv += v[i]*v[i];
	return vv;
}
template<typename T, typename R, typename S, int N> void vector_cross(T *c, const R *a, const S *b){
	if (3==N){
		c[0] = (R)(a[1]*b[2] - a[2]*b[1]);
		c[1] = (R)(a[2]*b[0] - a[0]*b[2]);
		c[2] = (R)(a[0]*b[1] - a[1]*b[0]);
	}
}
template<typename R, int N> R vector_dot(const R *a, const R *b){
	R out = 0;
	for (int i=0; i<N; i++) out += a[i]*b[i];
	return out;
}

template<typename T> T mod1(const T a){
	T b = a - cast_to_int(a);
	return ( (b < T(0) - ZERO_PREC ) ? b + T(1) : b );
}

template<typename T, typename R, int N> bool is_int_matrix(const T * A, const R tol){
	for (int i=0; i<N*N; i++) if ( my_abs(cast_to_int(A[i]) - A[i]) > tol ) return false;
	return true;
}
template<typename R> bool is_int_matrix(const int *, const R){ return true; }
