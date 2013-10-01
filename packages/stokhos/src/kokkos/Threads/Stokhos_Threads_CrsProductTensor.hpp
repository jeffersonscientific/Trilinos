// @HEADER
// ***********************************************************************
//
//                           Stokhos Package
//                 Copyright (2009) Sandia Corporation
//
// Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
// license for use of this work by or on behalf of the U.S. Government.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Eric T. Phipps (etphipp@sandia.gov).
//
// ***********************************************************************
// @HEADER

#ifndef STOKHOS_THREADS_CRS_PRODUCT_TENSOR_HPP
#define STOKHOS_THREADS_CRS_PRODUCT_TENSOR_HPP

#include "Kokkos_Threads.hpp"

#include "Stokhos_Multiply.hpp"
#include "Stokhos_CrsProductTensor.hpp"
#include "Stokhos_Threads_TinyVec.hpp"

namespace Stokhos {

template< typename ValueType >
class Multiply< CrsProductTensor< ValueType , Kokkos::Threads > ,
                void , void , DefaultSparseMatOps >
{
public:

  typedef Kokkos::Threads device_type;
  typedef CrsProductTensor< ValueType , device_type > tensor_type ;
  typedef typename tensor_type::size_type size_type ;

// Whether to use manual or auto-vectorization
#ifdef __MIC__
#define USE_AUTO_VECTORIZATION 1
#else
#define USE_AUTO_VECTORIZATION 0
#endif

#if defined(__INTEL_COMPILER) && USE_AUTO_VECTORIZATION

  // Version leveraging intel vectorization
  template< typename MatrixValue , typename VectorValue >
  static void apply( const tensor_type & tensor ,
                     const MatrixValue * const a ,
                     const VectorValue * const x ,
                           VectorValue * const y )
  {
    // The intel compiler doesn't seem to be able to vectorize through
    // the coord() calls, so extract pointers
    const size_type * cj = &tensor.coord(0,0);
    const size_type * ck = &tensor.coord(0,1);
    const size_type nDim = tensor.dimension();

    for ( size_type iy = 0 ; iy < nDim ; ++iy ) {
      const size_type nEntry = tensor.num_entry(iy);
      const size_type iEntryBeg = tensor.entry_begin(iy);
      const size_type iEntryEnd = iEntryBeg + nEntry;
      VectorValue ytmp = 0;

#pragma simd vectorlength(tensor_type::vectorsize)
#pragma ivdep
#pragma vector aligned
      for (size_type iEntry = iEntryBeg; iEntry<iEntryEnd; ++iEntry) {
        const size_type j    = cj[iEntry]; //tensor.coord(iEntry,0);
        const size_type k    = ck[iEntry]; //tensor.coord(iEntry,1);
        ytmp += tensor.value(iEntry) * ( a[j] * x[k] + a[k] * x[j] );
      }

      y[iy] += ytmp ;
    }
  }

#elif defined(__MIC__)

  // Version specific to MIC architecture using manual vectorization
  template< typename MatrixValue , typename VectorValue >
  static void apply( const tensor_type & tensor ,
                     const MatrixValue * const a ,
                     const VectorValue * const x ,
                           VectorValue * const y )
  {
    const size_type nDim = tensor.dimension();
    for ( size_type iy = 0 ; iy < nDim ; ++iy ) {

      const size_type nEntry = tensor.num_entry(iy);
      const size_type iEntryBeg = tensor.entry_begin(iy);
      const size_type iEntryEnd = iEntryBeg + nEntry;
            size_type iEntry    = iEntryBeg;

      VectorValue ytmp = 0 ;

      const size_type nBlock = nEntry / tensor_type::vectorsize;
      const size_type nEntryB = nBlock * tensor_type::vectorsize;
      const size_type iEnd = iEntryBeg + nEntryB;

      typedef TinyVec<ValueType,tensor_type::vectorsize,tensor_type::use_intrinsics> TV;
      TV vy;
      vy.zero();
      for (size_type block=0; block<nBlock; ++block, iEntry+=tensor_type::vectorsize) {
        const size_type *j = &tensor.coord(iEntry,0);
        const size_type *k = &tensor.coord(iEntry,1);
        TV aj(a, j), ak(a, k), xj(x, j), xk(x, k),
          c(&(tensor.value(iEntry)));

        // vy += c * ( aj * xk + ak * xj)
        aj.times_equal(xk);
        aj.multiply_add(ak, xj);
        vy.multiply_add(c, aj);

      }
      ytmp += vy.sum();

      const size_type rem = iEntryEnd-iEntry;
      if (rem > 8) {
        typedef TinyVec<ValueType,tensor_type::vectorsize,true,true> TV2;
        const size_type *j = &tensor.coord(iEntry,0);
        const size_type *k = &tensor.coord(iEntry,1);
        TV2 aj(a, j, rem), ak(a, k, rem), xj(x, j, rem), xk(x, k, rem),
          c(&(tensor.value(iEntry)), rem);

        // vy += c * ( aj * xk + ak * xj)
        aj.times_equal(xk);
        aj.multiply_add(ak, xj);
        aj.times_equal(c);
        ytmp += aj.sum();
        iEntry += rem;
      }

      else if (rem > 0) {
        typedef TinyVec<ValueType,8,true,true> TV2;
        const size_type *j = &tensor.coord(iEntry,0);
        const size_type *k = &tensor.coord(iEntry,1);
        TV2 aj(a, j, rem), ak(a, k, rem), xj(x, j, rem), xk(x, k, rem),
          c(&(tensor.value(iEntry)), rem);

        // vy += c * ( aj * xk + ak * xj)
        aj.times_equal(xk);
        aj.multiply_add(ak, xj);
        aj.times_equal(c);
        ytmp += aj.sum();
        iEntry += rem;
      }

      y[iy] += ytmp ;
    }
  }

#else

  // General version
  template< typename MatrixValue , typename VectorValue >
  static void apply( const tensor_type & tensor ,
                     const MatrixValue * const a ,
                     const VectorValue * const x ,
                           VectorValue * const y )
  {
    const size_type nDim = tensor.dimension();
    for ( size_type iy = 0 ; iy < nDim ; ++iy ) {

      const size_type nEntry = tensor.num_entry(iy);
      const size_type iEntryBeg = tensor.entry_begin(iy);
      const size_type iEntryEnd = iEntryBeg + nEntry;
            size_type iEntry    = iEntryBeg;

      VectorValue ytmp = 0 ;

      // Do entries with a blocked loop of size vectorsize
      if (tensor_type::vectorsize > 1 && nEntry >= tensor_type::vectorsize) {
        const size_type nBlock = nEntry / tensor_type::vectorsize;
        const size_type nEntryB = nBlock * tensor_type::vectorsize;
        const size_type iEnd = iEntryBeg + nEntryB;

        typedef TinyVec<ValueType,tensor_type::vectorsize,tensor_type::use_intrinsics> TV;
        TV vy;
        vy.zero();
        for (; iEntry<iEnd; iEntry+=tensor_type::vectorsize) {
          const size_type *j = &tensor.coord(iEntry,0);
          const size_type *k = &tensor.coord(iEntry,1);
          TV aj(a, j), ak(a, k), xj(x, j), xk(x, k), c(&(tensor.value(iEntry)));

          // vy += c * ( aj * xk + ak * xj)
          aj.times_equal(xk);
          aj.multiply_add(ak, xj);
          vy.multiply_add(c, aj);
        }
        ytmp += vy.sum();
      }

      // Do remaining entries with a scalar loop
      for ( ; iEntry<iEntryEnd; ++iEntry) {
        const size_type j = tensor.coord(iEntry,0);
        const size_type k = tensor.coord(iEntry,1);

        ytmp += tensor.value(iEntry) * ( a[j] * x[k] + a[k] * x[j] );
      }

      y[iy] += ytmp ;
    }
  }
#endif

  static size_type matrix_size( const tensor_type & tensor )
  { return tensor.dimension(); }

  static size_type vector_size( const tensor_type & tensor )
  { return tensor.dimension(); }
};

//----------------------------------------------------------------------------

// Specialization of Multiply< BlockCrsMatrix< BlockSpec, ... > > > for
// CrsProductTensor, which provides a version that processes blocks of FEM
// columns together to reduce the number of global reads of the sparse 3 tensor
template< typename ValueType , typename MatrixValue , typename VectorValue >
class Multiply<
  BlockCrsMatrix< StochasticProductTensor< ValueType, CrsProductTensor< ValueType , Kokkos::Threads > , Kokkos::Threads > , MatrixValue , Kokkos::Threads > ,
  Kokkos::View< VectorValue** , Kokkos::LayoutLeft , Kokkos::Threads > ,
  Kokkos::View< VectorValue** , Kokkos::LayoutLeft , Kokkos::Threads > ,
  DefaultSparseMatOps >
{
public:

  typedef Kokkos::Threads device_type ;
  typedef CrsProductTensor< ValueType , device_type > tensor_type;
  typedef StochasticProductTensor< ValueType, tensor_type, device_type > BlockSpec;
  typedef typename BlockSpec::size_type size_type ;
  typedef Kokkos::View< VectorValue** , Kokkos::LayoutLeft , device_type > block_vector_type ;
  typedef BlockCrsMatrix< BlockSpec , MatrixValue , device_type >  matrix_type ;

  const matrix_type  m_A ;
  const block_vector_type  m_x ;
  const block_vector_type  m_y ;

  Multiply( const matrix_type & A ,
            const block_vector_type & x ,
            const block_vector_type & y )
  : m_A( A )
  , m_x( x )
  , m_y( y )
  {}

  //--------------------------------------------------------------------------
  //  A( storage_size( m_A.block.size() ) , m_A.graph.row_map.size() );
  //  x( m_A.block.dimension() , m_A.graph.row_map.first_count() );
  //  y( m_A.block.dimension() , m_A.graph.row_map.first_count() );
  //

  inline
  void operator()( const size_type iBlockRow ) const
  {
    // Prefer that y[ m_A.block.dimension() ] be scratch space
    // on the local thread, but cannot dynamically allocate
    VectorValue * const y = & m_y(0,iBlockRow);

    const size_type iEntryBegin = m_A.graph.row_map[ iBlockRow ];
    const size_type iEntryEnd   = m_A.graph.row_map[ iBlockRow + 1 ];

    // Leading dimension guaranteed contiguous for LayoutLeft
    for ( size_type j = 0 ; j < m_A.block.dimension() ; ++j ) { y[j] = 0 ; }

    for ( size_type iEntry = iEntryBegin ; iEntry < iEntryEnd ; ++iEntry ) {
      const VectorValue * const x = & m_x( 0 , m_A.graph.entries(iEntry) );
      const MatrixValue * const a = & m_A.values( 0 , iEntry );

      Multiply< BlockSpec , void , void , DefaultSparseMatOps >::apply(
        m_A.block , a , x , y );
    }

  }

  /*
   * Compute work range = (begin, end) such that adjacent threads write to
   * separate cache lines
   */
  inline
  std::pair< size_type , size_type >
  compute_work_range( const size_type work_count ,
                      const size_type thread_count ,
                      const size_type thread_rank ) const
  {
    enum { work_align = 64 / sizeof(VectorValue) };
    enum { work_shift = Kokkos::Impl::power_of_two< work_align >::value };
    enum { work_mask  = work_align - 1 };

    const size_type work_per_thread =
      ( ( ( ( work_count + work_mask ) >> work_shift ) + thread_count - 1 ) /
        thread_count ) << work_shift ;

    const size_type work_begin =
      std::min( thread_rank * work_per_thread , work_count );
    const size_type work_end   =
      std::min( work_begin + work_per_thread , work_count );

    return std::make_pair( work_begin , work_end );
  }

#ifdef __MIC__

  // A MIC-specific version of the block-multiply algorithm, where block here
  // means processing multiple FEM columns at a time
  inline
  void operator()( device_type device ) const
  {
    const size_type iBlockRow = device.league_rank();

    // Check for valid row
    const size_type row_count = m_A.graph.row_map.dimension_0()-1;
    if (iBlockRow >= row_count)
      return;

    const size_type num_thread = device.team_size();
    const size_type thread_idx = device.team_rank();
    std::pair<size_type,size_type> work_range =
      compute_work_range(m_A.block.dimension(), num_thread, thread_idx);

    // Prefer that y[ m_A.block.dimension() ] be scratch space
    // on the local thread, but cannot dynamically allocate
    VectorValue * const y = & m_y(0,iBlockRow);

    // Leading dimension guaranteed contiguous for LayoutLeft
    for ( size_type j = work_range.first ; j < work_range.second ; ++j )
      y[j] = 0 ;

    const tensor_type& tensor = m_A.block.tensor();

    const size_type iBlockEntryBeg = m_A.graph.row_map[ iBlockRow ];
    const size_type iBlockEntryEnd = m_A.graph.row_map[ iBlockRow + 1 ];
    const size_type BlockSize = 9;
    const size_type numBlock =
      (iBlockEntryEnd-iBlockEntryBeg+BlockSize-1) / BlockSize;

    const MatrixValue* sh_A[BlockSize];
    const VectorValue* sh_x[BlockSize];

    size_type iBlockEntry = iBlockEntryBeg;
    for (size_type block = 0; block<numBlock; ++block, iBlockEntry+=BlockSize) {
      const size_type block_size =
        block == numBlock-1 ? iBlockEntryEnd-iBlockEntry : BlockSize;

      for ( size_type col = 0; col < block_size; ++col ) {
        const size_type iBlockColumn = m_A.graph.entries( iBlockEntry + col );
        sh_x[col] = & m_x( 0 , iBlockColumn );
        sh_A[col] = & m_A.values( 0 , iBlockEntry + col );
      }

      for ( size_type iy = work_range.first ; iy < work_range.second ; ++iy ) {

        const size_type nEntry = tensor.num_entry(iy);
        const size_type iEntryBeg = tensor.entry_begin(iy);
        const size_type iEntryEnd = iEntryBeg + nEntry;
              size_type iEntry    = iEntryBeg;

        VectorValue ytmp = 0 ;

        // Do entries with a blocked loop of size blocksize
        const size_type nBlock = nEntry / tensor_type::vectorsize;
        const size_type nEntryB = nBlock * tensor_type::vectorsize;
        const size_type iEnd = iEntryBeg + nEntryB;

        typedef TinyVec<ValueType,tensor_type::vectorsize,tensor_type::use_intrinsics> ValTV;
        typedef TinyVec<MatrixValue,tensor_type::vectorsize,tensor_type::use_intrinsics> MatTV;
        typedef TinyVec<VectorValue,tensor_type::vectorsize,tensor_type::use_intrinsics> VecTV;
        VecTV vy;
        vy.zero();
        for (size_type block=0; block<nBlock; ++block, iEntry+=tensor_type::vectorsize) {
          const size_type *j = &tensor.coord(iEntry,0);
          const size_type *k = &tensor.coord(iEntry,1);
          ValTV c(&(tensor.value(iEntry)));

          for ( size_type col = 0; col < block_size; ++col ) {
            MatTV aj(sh_A[col], j), ak(sh_A[col], k);
            VecTV xj(sh_x[col], j), xk(sh_x[col], k);

            // vy += c * ( aj * xk + ak * xj)
            aj.times_equal(xk);
            aj.multiply_add(ak, xj);
            vy.multiply_add(c, aj);
          }
        }
        ytmp += vy.sum();

        const size_type rem = iEntryEnd-iEntry;
        if (rem > 8) {
          typedef TinyVec<ValueType,tensor_type::vectorsize,true,true> ValTV2;
          typedef TinyVec<MatrixValue,tensor_type::vectorsize,true,true> MatTV2;
          typedef TinyVec<VectorValue,tensor_type::vectorsize,true,true> VecTV2;
          const size_type *j = &tensor.coord(iEntry,0);
          const size_type *k = &tensor.coord(iEntry,1);
          ValTV2 c(&(tensor.value(iEntry)), rem);

          for ( size_type col = 0; col < block_size; ++col ) {
            MatTV2 aj(sh_A[col], j, rem), ak(sh_A[col], k, rem);
            VecTV2 xj(sh_x[col], j, rem), xk(sh_x[col], k, rem);

            // vy += c * ( aj * xk + ak * xj)
            aj.times_equal(xk);
            aj.multiply_add(ak, xj);
            aj.times_equal(c);
            ytmp += aj.sum();
            iEntry += rem;
          }
        }

        else if (rem > 0) {
          typedef TinyVec<ValueType,8,true,true> ValTV2;
          typedef TinyVec<MatrixValue,8,true,true> MatTV2;
          typedef TinyVec<VectorValue,8,true,true> VecTV2;
          const size_type *j = &tensor.coord(iEntry,0);
          const size_type *k = &tensor.coord(iEntry,1);
          ValTV2 c(&(tensor.value(iEntry)), rem);

          for ( size_type col = 0; col < block_size; ++col ) {
            MatTV2 aj(sh_A[col], j, rem), ak(sh_A[col], k, rem);
            VecTV2 xj(sh_x[col], j, rem), xk(sh_x[col], k, rem);

            // vy += c * ( aj * xk + ak * xj)
            aj.times_equal(xk);
            aj.multiply_add(ak, xj);
            aj.times_equal(c);
            ytmp += aj.sum();
            iEntry += rem;
          }
        }

        y[iy] += ytmp ;
      }

      // Add a team barrier to keep the thread team in-sync before going on
      // to the next block
      device.team_barrier();
    }

  }

#else

  // A general hand-vectorized version of the block multiply algorithm, where
  // block here means processing multiple FEM columns at a time.  Note that
  // auto-vectorization of a block algorithm doesn't work, because the
  // stochastic loop is not the inner-most loop.
  inline
  void operator()( device_type device ) const
  {
    const size_type iBlockRow = device.league_rank();

    // Check for valid row
    const size_type row_count = m_A.graph.row_map.dimension_0()-1;
    if (iBlockRow >= row_count)
      return;

    const size_type num_thread = device.team_size();
    const size_type thread_idx = device.team_rank();
    std::pair<size_type,size_type> work_range =
      compute_work_range(m_A.block.dimension(), num_thread, thread_idx);

    // Prefer that y[ m_A.block.dimension() ] be scratch space
    // on the local thread, but cannot dynamically allocate
    VectorValue * const y = & m_y(0,iBlockRow);

    // Leading dimension guaranteed contiguous for LayoutLeft
    for ( size_type j = work_range.first ; j < work_range.second ; ++j )
      y[j] = 0 ;

    const tensor_type& tensor = m_A.block.tensor();

    const size_type iBlockEntryBeg = m_A.graph.row_map[ iBlockRow ];
    const size_type iBlockEntryEnd = m_A.graph.row_map[ iBlockRow + 1 ];
    const size_type BlockSize = 14;
    const size_type numBlock =
      (iBlockEntryEnd-iBlockEntryBeg+BlockSize-1) / BlockSize;

    const MatrixValue* sh_A[BlockSize];
    const VectorValue* sh_x[BlockSize];

    size_type iBlockEntry = iBlockEntryBeg;
    for (size_type block = 0; block<numBlock; ++block, iBlockEntry+=BlockSize) {
      const size_type block_size =
        block == numBlock-1 ? iBlockEntryEnd-iBlockEntry : BlockSize;

      for ( size_type col = 0; col < block_size; ++col ) {
        const size_type iBlockColumn = m_A.graph.entries( iBlockEntry + col );
        sh_x[col] = & m_x( 0 , iBlockColumn );
        sh_A[col] = & m_A.values( 0 , iBlockEntry + col );
      }

      for ( size_type iy = work_range.first ; iy < work_range.second ; ++iy ) {

        const size_type nEntry = tensor.num_entry(iy);
        const size_type iEntryBeg = tensor.entry_begin(iy);
        const size_type iEntryEnd = iEntryBeg + nEntry;
              size_type iEntry    = iEntryBeg;

        VectorValue ytmp = 0 ;

        // Do entries with a blocked loop of size blocksize
        if (tensor_type::vectorsize > 1 && nEntry >= tensor_type::vectorsize) {
          const size_type nBlock = nEntry / tensor_type::vectorsize;
          const size_type nEntryB = nBlock * tensor_type::vectorsize;
          const size_type iEnd = iEntryBeg + nEntryB;

          typedef TinyVec<ValueType,tensor_type::vectorsize,tensor_type::use_intrinsics> ValTV;
          typedef TinyVec<MatrixValue,tensor_type::vectorsize,tensor_type::use_intrinsics> MatTV;
          typedef TinyVec<VectorValue,tensor_type::vectorsize,tensor_type::use_intrinsics> VecTV;
          VecTV vy;
          vy.zero();
          for (; iEntry<iEnd; iEntry+=tensor_type::vectorsize) {
            const size_type *j = &tensor.coord(iEntry,0);
            const size_type *k = &tensor.coord(iEntry,1);
            ValTV c(&(tensor.value(iEntry)));

            for ( size_type col = 0; col < block_size; ++col ) {
              MatTV aj(sh_A[col], j), ak(sh_A[col], k);
              VecTV xj(sh_x[col], j), xk(sh_x[col], k);

              // vy += c * ( aj * xk + ak * xj)
              aj.times_equal(xk);
              aj.multiply_add(ak, xj);
              vy.multiply_add(c, aj);
            }
          }
          ytmp += vy.sum();
        }

        // Do remaining entries with a scalar loop
        for ( ; iEntry<iEntryEnd; ++iEntry) {
          const size_type j = tensor.coord(iEntry,0);
          const size_type k = tensor.coord(iEntry,1);
          ValueType cijk = tensor.value(iEntry);

          for ( size_type col = 0; col < block_size; ++col ) {
            ytmp += cijk * ( sh_A[col][j] * sh_x[col][k] +
                             sh_A[col][k] * sh_x[col][j] );
          }

        }

        y[iy] += ytmp ;
      }

      // Add a team barrier to keep the thread team in-sync before going on
      // to the next block
      device.team_barrier();
    }

  }

#endif

  static void apply( const matrix_type & A ,
                     const block_vector_type & x ,
                     const block_vector_type & y )
  {
    // Generally the block algorithm seems to perform better on the MIC,
    // as long as the stochastic size isn't too big, but doesn't perform
    // any better on the CPU (probably because the CPU has a fat L3 cache
    // to store the sparse 3 tensor).
#ifdef __MIC__
    const bool use_block_algorithm = true;
#else
    const bool use_block_algorithm = false;
#endif

    const size_t row_count = A.graph.row_map.dimension_0() - 1 ;
    if (use_block_algorithm) {
      typedef typename matrix_type::device_type device_type;
      const size_t team_size = device_type::team_max();
      const size_t league_size = row_count;
      Kokkos::ParallelWorkRequest config(league_size, team_size);
      Kokkos::parallel_for( config , Multiply(A,x,y) );
    }
    else {
      Kokkos::parallel_for( row_count , Multiply(A,x,y) );
    }
  }
};

} // namespace Stokhos

#endif /* #ifndef STOKHOS_THREADS_CRS_PRODUCT_TENSOR_HPP */
