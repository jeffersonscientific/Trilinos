#include "Tpetra_Core.hpp"
#include "Teuchos_RCP.hpp"
#include "graph.h"

#include <string>
#include <sstream>
#include <iostream>
#include <queue>

#ifndef ZOLTAN2_VTXLABEL_
#define ZOLTAN2_VTXLABEL_

namespace iceProp{
	std::queue<int> art;
	std::queue<int> reg;


	enum Grounding_Status {FULL=2, HALF=1, NONE = 0};
	// Struct representing a vertex label.
	// We define our own "addition" for these labels.
	// Later, we'll create a Tpetra::FEMultiVector of these labels.
	class vtxLabel {
	public:
	  //int gid;
          int id;
	  int first_label;
	  int first_sender;
	  int second_label;
	  int second_sender;
	  bool is_art;
	  // Constructors
	  vtxLabel(int idx, int first = -1, int first_sender = -1, int second = -1, int second_sender = -1,bool art = false) { 
	    id = idx;
            //gid = idx;
	    if(id == -1){
		std::cout<<"A label's ID is -1\n";
	    }
	    first_label = first;
	    this->first_sender = first_sender;
	    second_label = second;
	    this->second_sender = second_sender; 
	    is_art = art;
	  }
	  vtxLabel() {
	    id = -1;
	    first_label = -1;
	    first_sender = -1;
	    second_label = -1;
	    second_sender = -1;
	    is_art = false; 
	  }
	  // vtxLabel assignment
	  vtxLabel& operator=(const vtxLabel& other)  { 
	    id = other.id;
            //gid = other.gid;
	    first_label = other.first_label;
	    first_sender = other.first_sender;
	    second_label = other.second_label;
	    second_sender = other.second_sender;
	    is_art = other.is_art;
	    return *this;
	  }
	  // int assignment
	  vtxLabel& operator=(const int& other) { 
	    first_label = other;
	    first_sender = other;
	    second_label = -1;
	    second_sender = -1;
	    return *this;
	  }
	  // += overload
	  // for communicating copy's labels over processor boundaries.
          vtxLabel& operator+=(const vtxLabel& copy) {
	    Grounding_Status owned_gs = getGroundingStatus();
            Grounding_Status copy_gs = copy.getGroundingStatus();
            //The only cases we care about are 
            //owned	copy	
            //NONE  <   HALF
            //NONE  <	FULL
            //HALF  ==	HALF
            //HALF  <	FULL
          
	    //handles NONE < HALF, HALF < FULL
            if(owned_gs < copy_gs){
              first_label = copy.first_label;
              first_sender = copy.first_sender;
              second_label = copy.second_label;
              second_sender = copy.second_sender;
            //handles HALF == HALF
            } else if(owned_gs == copy_gs && owned_gs == HALF){
              if(copy.first_label != first_label){
                second_label = copy.first_label;
                second_sender = copy.first_sender;
              }
            }
            
            if(getGroundingStatus() != owned_gs){
              if(!is_art) iceProp::reg.push(id);
              else iceProp::art.push(id);
            }
            
	    return *this;
	  }
	  // addition overload
	  /*friend vtxLabel operator+(const vtxLabel& lhs, const vtxLabel& rhs) {
	   
	    vtxLabel result(-1,-1,-1,-1,-1,false);
	    return result;
	  }*/
	  // vtxLabel equality overload
	  friend bool operator==(const vtxLabel& lhs, const vtxLabel& rhs) {
	    return ((lhs.first_label == rhs.first_label)&&(lhs.first_sender == rhs.first_sender)&&(lhs.second_label == rhs.second_label)&&(lhs.second_sender == rhs.second_sender));
	  }
	  // int equality overload
	  friend bool operator==(const vtxLabel& lhs, const int& rhs) {
	    return ((lhs.first_label == rhs)&&(lhs.first_sender == rhs));
	  }
	  // output stream overload
	  friend std::ostream& operator<<(std::ostream& os, const vtxLabel& a) {
	    os<<a.id<<": "<< a.first_label<<", "<<a.first_sender<<"; "<<a.second_label<<", "<<a.second_sender;
	    return os;
	  }
	  Grounding_Status getGroundingStatus() const {
	    return (Grounding_Status)((first_label != -1) + (second_label != -1));
	  }
	};

}//end namespace iceProp
        
/////////////////////////////////////////////////////////////////////////
// ArithTraits -- arithmetic traits needed for struct vtxLabel
// Needed so that Tpetra compiles.
// Not all functions were needed; this is a subset of ArithTraits' traits.
// Modified from kokkos-kernels/src/Kokkos_ArithTraits.hpp's 
// <int> specialization
namespace Kokkos {
  namespace Details {

    template<>
    class ArithTraits<iceProp::vtxLabel> {  // specialized for vtxLabel struct
    public:
      typedef iceProp::vtxLabel val_type;
      typedef int mag_type;
    
      static const bool is_specialized = true;
      static const bool is_signed = true;
      static const bool is_integer = true;
      static const bool is_exact = true;
      static const bool is_complex = false;
    
      static KOKKOS_FORCEINLINE_FUNCTION bool isInf(const val_type &) {
	return false;
      }
      static KOKKOS_FORCEINLINE_FUNCTION bool isNan(const val_type &) {
	return false;
      }
      static KOKKOS_FORCEINLINE_FUNCTION mag_type abs(const val_type &x) {
	return (x.first_label >= 0 ? x.first_label : -(x.first_label));
      }
      static KOKKOS_FORCEINLINE_FUNCTION mag_type zero() { return 0; }
      static KOKKOS_FORCEINLINE_FUNCTION mag_type one() { return 1; }
      static KOKKOS_FORCEINLINE_FUNCTION mag_type min() { return INT_MIN; }
      static KOKKOS_FORCEINLINE_FUNCTION mag_type max() { return INT_MAX; }
      static KOKKOS_FORCEINLINE_FUNCTION mag_type nan() { return -1; }
    
      // Backwards compatibility with Teuchos::ScalarTraits.
      typedef mag_type magnitudeType;
      static const bool isComplex = false;
      static const bool isOrdinal = true;
      static const bool isComparable = true;
      static const bool hasMachineParameters = false;
      static KOKKOS_FORCEINLINE_FUNCTION magnitudeType magnitude(
	const val_type &x) 
      {
	return abs(x);
      }
      static KOKKOS_FORCEINLINE_FUNCTION bool isnaninf(const val_type &) {
	return false;
      }
      static std::string name() { return "iceProp::vtxLabel"; }
    };
  }
}

/////////////////////////////////////////////////////////////////////////////
// Teuchos::SerializationTraits are needed to copy vtxLabels into MPI buffers
// Because sizeof(vtxLabel) works for struct vtxLabel, we'll use a 
// provided serialization of vtxLabel into char*.
namespace Teuchos {
template<typename Ordinal>
struct SerializationTraits<Ordinal, iceProp::vtxLabel> :
       public Teuchos::DirectSerializationTraits<Ordinal, iceProp::vtxLabel>
{};
}//end namespace Teuchos

namespace iceProp{
class iceSheetPropagation {
public:
  //typedefs for the FEMultiVector
  typedef Tpetra::Map<> map_t;
  typedef map_t::local_ordinal_type lno_t;
  typedef map_t::global_ordinal_type gno_t;
  typedef vtxLabel scalar_t;
  typedef Tpetra::FEMultiVector<scalar_t,lno_t, gno_t> femv_t;	
  
  
 	
  //Constructor assigns vertices to processors and builds maps with and
  //without copies
  iceSheetPropagation(Teuchos::RCP<const Teuchos::Comm<int> > &comm_, Teuchos::RCP<const map_t> mapOwned_, Teuchos::RCP<const map_t> mapWithCopies_, graph* g_,int* boundary_flags, int* grounding_flags,int localOwned,int localCopy):
    me(comm_->getRank()), np(comm_->getSize()),
    nLocalOwned(localOwned), nLocalCopy(localCopy),
    nVec(1), comm(comm_),g(g_),mapOwned(mapOwned_),
    mapWithCopies(mapWithCopies_)
  {
    //Each rank has 15 IDs, the last five of which overlap with the next rank.
    //(IDs and owning processors wrap-around from processor np-1 to 0)
    /*const Tpetra::global_size_t nGlobal = np * nLocalOwned;
    lno_t offset = me * nLocalOwned;
    
    Teuchos::Array<gno_t> gids(nLocalOwned + nLocalCopy);
    for(lno_t i = 0; i < nLocalOwned + nLocalCopy; i++)
      gids[i] = static_cast<gno_t> (offset + i) % nGlobal;
    
    //Create Map of owned + copies (a.k.a. overlap map); analogous to ColumnMap
    Tpetra::global_size_t dummy = Teuchos::OrdinalTraits<Tpetra::global_size_t>::invalid();
    mapWithCopies = rcp(new map_t(dummy, gids(), 0, comm));

    //create map of owned only (a.k.a. one-to-one map); analogous to RowMap
    mapOwned = rcp(new map_t(dummy, gids(0,nLocalOwned), 0, comm));*/

    //print the entries of each map
    //std::cout << me << " MAP WITH COPIES ("
    //                << mapWithCopies->getGlobalNumElements() <<"):   ";
    //lno_t nlocal = lno_t(mapWithCopies->getNodeNumElements());
    //for(lno_t i = 0; i < nlocal; i++)
    //  std::cout << mapWithCopies->getGlobalElement(i) << " ";
    //std::cout << std::endl;

    typedef Tpetra::Import<lno_t, gno_t> import_t;
    Teuchos::RCP<import_t> importer = rcp(new import_t(mapOwned, mapWithCopies));
    
    femv = rcp(new femv_t(mapOwned, importer, nVec, true));
   
    
 
    //this is where the algorithm goes. Things that I still need to do:
    //	1. initialize the femv with the vtxLabels, using input data
    //	2. add half-grounded nodes to iceProp::reg
    //	3. call propagate.

    femv->beginFill();
    //set the member variable that stores femv->getData(0);
    femvData = femv->getData(0);
    for(lno_t i = 0; i < nLocalOwned + nLocalCopy; i++){
      gno_t gid = mapWithCopies->getGlobalElement(i);
      iceProp::vtxLabel label(i);
      if(boundary_flags[i] > 2) label.is_art = true;
      if(grounding_flags[i]){
        label.first_label = gid;
        label.first_sender = gid;
        iceProp::reg.push(label.id);
        //if(label.is_art) iceProp::art.push(label.id);
        //else iceProp::reg.push(label.id);
      }
      femv->replaceLocalValue(i,0,label);
      //femv->replaceGlobalValue(gid,1,me);
    }
    //iceProp::vtxLabel label1 = femv->getData(0)[0];
    //iceProp::vtxLabel label2 = femv->getData(0)[1];
    //label1 += label2;

    //femv->replaceGlobalValue(0,0,label1);
    //printFEMV(*femv, "BeforeFill");   
 
    femv->endFill(); 
    //printFEMV(*femv, "AfterFill");
  }
  void printFEMV(femv_t &femv, const char *msg){
    for (int v = 0; v < 1; v++){
      std::cout << me << " OWNED " << msg << " FEMV[" << v << "] Owned: ";
      auto value = femv.getData(v);
      for(lno_t i = 0; i < nLocalOwned; i++) std::cout<<value[i]<<" ";
      std::cout<<std::endl;
    }
    femv.switchActiveMultiVector();
    for(int v = 0; v < 1; v++){
      std::cout << me << " WITHCOPIES " <<msg<<" FEMV[" <<v<< "] Owned: ";
      auto value = femv.getData(v);
      for(lno_t i = 0; i < nLocalOwned; i++) std::cout << value[i] <<" ";
      std::cout<<" Copies: ";
      for(lno_t i = nLocalOwned; i < nLocalOwned+nLocalCopy; i++)
        std::cout << value[i] <<" ";
      std::cout << std::endl;
    }
    femv.switchActiveMultiVector();

  }
  //propagation functions
  //returns an array of vertices to remove
  int* propagate(void){ 
    //run bfs_prop
    bfs_prop(femv);
    //check for potentially false articulation points
    while(true){
      femv->switchActiveMultiVector(); 
      for(int i = 0; i < g->n; i++){
        vtxLabel curr_node = femvData[i];
        if(curr_node.is_art && curr_node.getGroundingStatus() == FULL){
          int out_degree = out_degree(g, curr_node.id);
          int* outs = out_vertices(g, curr_node.id);
          for(int j = 0; j < out_degree; j++){
            vtxLabel neighbor = femvData[outs[j]];
            if(neighbor.getGroundingStatus() == HALF && neighbor.first_label != curr_node.id && neighbor.first_sender == curr_node.id){
              iceProp::reg.push(curr_node.id);
            }
          }
        }
      }
      int local_done = iceProp::reg.empty();
      int done = 0;
      Teuchos::reduceAll<int,int>(*comm,Teuchos::REDUCE_MIN,1, &local_done,&done);
      
      if(done) break;

      for(int i = 0; i < g->n; i++){
        vtxLabel curr_node = femvData[i];
        if(curr_node.getGroundingStatus() == HALF){
          vtxLabel cleared(curr_node.id);
          cleared.is_art = curr_node.is_art;
          femv->replaceLocalValue(i,0,cleared);
        }
      }
      //std::cout<<me<<": Running BFS-prop again\n";
      //re-run bfs_prop until incomplete propagation is fixed
      bfs_prop(femv);     
    }
    //check for nodes that are less than full.
    //return flags for each node, -2 for keep, -1 for remove, <vtxID> for singly grounded nodes.
    int* removed = new int[g->n];
    for(int i = 0; i < g->n; i++){
      vtxLabel curr_node = femvData[i];
      Grounding_Status gs = curr_node.getGroundingStatus();
      if(gs == FULL) removed[i] = -2;
      else if(gs == HALF) removed[i] = curr_node.first_label;
      else removed[i] = -1;
    }
    femv->switchActiveMultiVector();
    //printFEMV(*femv, "AfterPropagation");
    return removed;
  }

  //performs one bfs_prop iteration (does not check for incomplete propagation)
  void bfs_prop(Teuchos::RCP<femv_t> femv){
    //do
    //  femv->beginFill()
    //  //call giveLabels in a while loop
    //  femv->endFill()
    //  reduceAll()
    //while(!done);
    int done = 0;
    while(!done){
      //std::cout<<me<<": Propagating...\n";
      femv->beginFill();
      //visit every node that changed
      std::queue<int>* curr;
      if(iceProp::reg.empty()) curr = &iceProp::art;
      else curr = &iceProp::reg;
      while(!curr->empty()){
        vtxLabel curr_node = femvData[curr->front()];
        curr->pop();
        int out_degree = out_degree(g, curr_node.id);
        int* outs = out_vertices(g, curr_node.id);
        for(int i = 0; i < out_degree; i++){
          vtxLabel neighbor = femvData[outs[i]];
          Grounding_Status old_gs = neighbor.getGroundingStatus();
          
          //give curr_node's neighbor some more labels
          giveLabels(curr_node, neighbor);
          
          if(old_gs != neighbor.getGroundingStatus()){
            femv->replaceLocalValue(outs[i],0,neighbor);
            if(neighbor.is_art) iceProp::art.push(neighbor.id);
            else iceProp::reg.push(neighbor.id);
          }
        }
        if(curr->empty()){
          if(curr == &iceProp::reg) curr = &iceProp::art;
          else curr = &iceProp::reg;
        }
        
      }
      //std::cout<<me<<": art queue front = "<<iceProp::art.front()<<"\n";
      //std::cout<<me<<": art queue size = "<<iceProp::art.size()<<"\n";
      femv->endFill();
      femv->doSourceToTarget(Tpetra::ADD);
      //std::cout<<me<<": reg queue front = "<<iceProp::reg.front()<<"\n";
      //std::cout<<me<<": reg queue size = "<<iceProp::reg.size()<<"\n";
      int local_done = iceProp::reg.empty() && iceProp::art.empty();
      //printFEMV(*femv,"Propagation Step");
      //this call makes sure that if any inter-processor communication changed labels
      //we catch the changes and keep propagating them.
      Teuchos::reduceAll<int,int>(*comm,Teuchos::REDUCE_MIN,1, &local_done,&done);
    }
    
    
  }
  
  //function that exchanges labels between two nodes
  //curr_node gives its labels to neighbor.
  void giveLabels(vtxLabel& curr_node, vtxLabel& neighbor){
    Grounding_Status curr_gs = curr_node.getGroundingStatus();
    Grounding_Status nbor_gs = neighbor.getGroundingStatus();
    int curr_node_gid = mapWithCopies->getGlobalElement(curr_node.id);
    //if the neighbor is full, we don't need to pass labels
    if(nbor_gs == FULL) return;
    //if the current nod is empty (shouldn't happen) we can't pass any labels
    if(curr_gs == NONE) return;
    //if the current node is full (and not an articulation point), pass both labels on
    if(curr_gs == FULL && !curr_node.is_art){
      neighbor.first_label = curr_node.first_label;
      neighbor.first_sender = curr_node_gid;
      neighbor.second_label = curr_node.second_label;
      neighbor.second_sender = curr_node_gid;
      return;
    } else if (curr_gs == FULL) {
      //if it is an articulation point, and it hasn't sent to this neighbor
      if(neighbor.first_sender != curr_node_gid){
        //send itself as a label
        if(nbor_gs == NONE){
          neighbor.first_label = curr_node_gid;
          neighbor.first_sender = curr_node_gid;
        } else if(nbor_gs == HALF){
          if(neighbor.first_label != curr_node_gid){
            neighbor.second_label = curr_node_gid;
            neighbor.second_sender = curr_node_gid;
          }
        }
      }
      return;
    }
    //if the current node has only one label
    if(curr_gs == HALF){
      //pass that on appropriately
      if(nbor_gs == NONE){
        neighbor.first_label = curr_node.first_label;
        neighbor.first_sender = curr_node_gid;
      } else if(nbor_gs == HALF){
        //make sure you aren't giving a duplicate label, and that
        //you haven't sent a label to this neighbor before.
        if(neighbor.first_label != curr_node.first_label && neighbor.first_sender != curr_node_gid){
          neighbor.second_label = curr_node.first_label;
          neighbor.second_sender = curr_node_gid;
        }
      }
    }
    
    if(nbor_gs != neighbor.getGroundingStatus()){
      if(neighbor.is_art) iceProp::art.push(neighbor.id);
      else iceProp::reg.push(neighbor.id);
    } 
  }
  
  void bccPropagate(){
    //while (there are empty vertices)
      //initialize the reg frontier
      //(also count how many empty vertices there are)
      //call this->propagate
      //check for articulation points
    //endwhile
  }
  
  int vtxLabelUnitTest();

private:
  graph* g;	    //csr representation of vertices on this processor
  int me; 	    //my processor rank
  int np;	    //number of processors
  int nLocalOwned;  //number of vertices owned by this processor
  int nLocalCopy;   //number of copies of off-processor vertices on this proc
  int nVec;	    //number of vectors in multivector
  
  Teuchos::RCP<const Teuchos::Comm<int> > comm; //MPI communicator

  Teuchos::RCP<const map_t> mapWithCopies;  //Tpetra::Map including owned
                                            //vertices and copies
  Teuchos::RCP<const map_t> mapOwned;       //Tpetra::Map including only owned

  Teuchos::RCP<femv_t> femv;
  
  Teuchos::ArrayRCP<const scalar_t> femvData;
};
}//end namespace iceProp

//  Unit test for the vtxLabel struct
//  Make sure vtxLabel's overloaded operators compile and work as expected.
//
//  Tests for coverage of += operator	done
//  	empty non-art += empty non-art    x
//  	empty non-art += half non-art     x
//  	empty non-art += full non-art	  x
//  	empty non-art += empty art	  x
//  	empty non-art += half art         x
//  	empty non-art += full art         x
//  	half non-art += empty non-art     x
//  	half non-art += half non-art   	  x
//	half non-art += full non-art      x
//	half non-art += empty art	  x
//	half non-art += half art	  x
//	half non-art += full art	  x
//	full non-art += empty non-art     x
//	full non-art += half non-art      x
//	full non-art += full non-art	  x
//	full non-art += empty art 	  x
//	full non-art += half art          x
//	full non-art += full art          x
//  
//  Note: suspected articulation points (SAPs?) receive labels in the 
//  same way that regular vertices do, so the only cases we need to test
//  for exercising the articulation point logic are the cases where
//  suspected articulation points are on the right hand side of the +=.
//
int  iceProp::iceSheetPropagation::vtxLabelUnitTest()
{
  int ierr = 0;
  iceProp::vtxLabel a(0);
  iceProp::vtxLabel a_same(0,-1,-1,-1,-1,false);
  
  //equality test 
  if(!(a == a_same)){
    std::cout<<"UnitTest Error: default constructor arguments not as expected "<<a<<" != "<<a_same<<"\n";
    ierr++;
  }

//  	empty non-art += empty non-art
  iceProp::vtxLabel ena1(0);
  iceProp::vtxLabel ena2(2);
  iceProp::vtxLabel result(0);
  giveLabels(ena2,ena1);
  
  if(!(ena1==result)){
    std::cout<<"UnitTest Error: empty non-art += empty non-art not as expected: "<<ena1<<" != "<<result<<"\n";
    ierr++;
  }

//  	empty non-art += half non-art 
  iceProp::vtxLabel hna1(0,1,1,-1,-1,false);
  ena1 = iceProp::vtxLabel(2);
  
  if(!iceProp::reg.empty()){
    std::cout<<"UnitTest Error: regular queue is not empty, but it should be\n";
    ierr++;
  }

  giveLabels(hna1,ena1);
  result = iceProp::vtxLabel(2,1,0,-1,-1,false);
  
  if(!(ena1 == result)){
    std::cout<<"UnitTest Error: empty non-art += half non-art not as expected "<<ena1<<" != "<<result<<"\n";
    ierr++;
  }

  if(iceProp::reg.front() != 2){
    std::cout<<"UnitTest Error: += did not update the regular queue: size is "<<iceProp::reg.size()<<", front is "<<iceProp::reg.front()<<"\n";
    ierr++;
  }
  
//  	empty non-art += full non-art
  ena1 = iceProp::vtxLabel(2);
  iceProp::vtxLabel fna1(10,3,5,7,9,false);
  result = iceProp::vtxLabel(2,3,10,7,10,false);

  //ena1 += fna1;
  giveLabels(fna1,ena1);
  if(!(ena1 == result)){
    std::cout<<"UnitTest Error: empty non-art += full non-art not as expected: "<<ena1<<" != "<<result<<"\n";
    ierr++;
  }

//  	empty non-art += empty art
  ena1 = iceProp::vtxLabel(2);
  iceProp::vtxLabel ea1(10,-1,-1,-1,-1,true);
  result = iceProp::vtxLabel(2);

  //ena1 += ea1;
  giveLabels(ea1,ena1);
  if(!(result == ena1)){
    std::cout<<"UnitTest Error: empty non-art += empty art not as expected: "<<ena1<<" != "<<result<<"\n";
    ierr++;
  }
  
//  	empty non-art += half art
  ena1 = iceProp::vtxLabel(2);
  iceProp::vtxLabel ha1(10,3,4,-1,-1,true);
  result = iceProp::vtxLabel(2,3,10,-1,-1,false);

  //ena1 += ha1;
  giveLabels(ha1,ena1);
  
  if(!(ena1 == result)){
    std::cout<<"UnitTest Error: empty non-art += half art not as expected: "<<ena1<<" != "<<result<<"\n";
    ierr++;
  }

//  	empty non-art += full art
  ena1 = iceProp::vtxLabel(2);
  iceProp::vtxLabel fa1(10,3,4,7,8,true);
  result = iceProp::vtxLabel(2,10,10,-1,-1,false);
 
  //ena1 += fa1;
  giveLabels(fa1,ena1);
  
  if(!(ena1 == result)){
    std::cout<<"UnitTest Error: empty non-art += full art not as expected: "<<ena1<<" != "<<result<<"\n";
    ierr++;
  }
  
//  	half non-art += empty non-art
  hna1 = iceProp::vtxLabel(2,3,5,-1,-1,false);
  ena1 = iceProp::vtxLabel(10);
  result = iceProp::vtxLabel(2,3,5,-1,-1,false);

  //hna1 += ena1;
  giveLabels(ena1,hna1);
  
  if(!(hna1 == result)){
    std::cout<<"UnitTest Error: half non-art += empty non-art not as expected: "<<hna1<<" != "<<result<<"\n";
    ierr++;
  }

//  	half non-art += half non-art   	  
  //same labels (senders don't matter in this case)
  hna1 = iceProp::vtxLabel(2,3,5,-1,-1,false);
  iceProp::vtxLabel hna2(4,3,9,-1,-1,false);
  result = iceProp::vtxLabel(2,3,5,-1,-1,false);
  
  //hna1+=hna2;
  giveLabels(hna2, hna1);

  if(!(hna1 == result)){
    std::cout<<"UnitTest Error: half non-art += half non-art with same labels not as expected: "<<hna1<<" != "<<result<<"\n";
    ierr++;
  }

  //hna2 has already sent to hna1
  hna1 = iceProp::vtxLabel(2,3,5,-1,-1,false);
  hna2 = iceProp::vtxLabel(5,3,9,-1,-1,false);
  result = iceProp::vtxLabel(2,3,5,-1,-1,false);
  
  //hna1 += hna2;
  giveLabels(hna2,hna1);

  if(!(result == hna1)){
    std::cout<<"UnitTest Error: half non-art += half non-art where rhs already sent to lhs not as expected: "<<hna1<<" != "<<result<<"\n";
    ierr++;
  }
  
  //different labels
  hna1 = iceProp::vtxLabel(2,3,5,-1,-1,false);
  hna2 = iceProp::vtxLabel(4,7,9,-1,-1,false);
  result = iceProp::vtxLabel(2,3,5,7,4,false);

  //hna1 += hna2;
  giveLabels(hna2,hna1);

  if(!(hna1 == result)){
    std::cout<<"UnitTest Error: half non-art += half non-art with different labels not as expected: "<<hna1<<" != "<<result<<"\n";
    ierr++;
  }
    
//	half non-art += full non-art
  //different labels, different senders
  hna1 = iceProp::vtxLabel(2,3,5,-1,-1,false);
  fna1 = iceProp::vtxLabel(6,4,9,1,0,false);
  result = iceProp::vtxLabel(2,4,6,1,6,false);

  //hna1 += fna1;
  giveLabels(fna1,hna1);
  
  if(!(result == hna1)){
    std::cout<<"UnitTest Error: half non-art += full non-art with different labels not as expected: "<<hna1<<" != "<<result<<"\n";
    ierr++;
  }

  //full sent the half's only label
  hna1 = iceProp::vtxLabel(2,3,5,-1,-1,false);
  fna1 = iceProp::vtxLabel(5,3,3,6,0,false);
  result = iceProp::vtxLabel(2,3,5,6,5,false);
  
  //hna1 += fna1;
  giveLabels(fna1,hna1);  

  if(!(result == hna1)){
    std::cout<<"UnitTest Error: half non-art += full non-art where full already sent half one label not as expected: "<<hna1<<" != "<<result<<"\n";
    ierr++;
  } 
  //full and half have one label in common
  hna1 = iceProp::vtxLabel(2,3,5,-1,-1,false);
  fna1 = iceProp::vtxLabel(1,3,6,8,0,false);
  result = iceProp::vtxLabel(2,3,1,8,1,false);
  
  //hna1 += fna1;
  giveLabels(fna1,hna1);  

  if(!(result == hna1)){
    std::cout<<"UnitTest Error: half non-art += full non-art where full & half share one label not as expected: "<<hna1 <<" !=  " <<result<<"\n";
    ierr++;
  }
  
//	half non-art += empty art
  hna1 = iceProp::vtxLabel(2,3,5,-1,-1,false);
  ea1 = iceProp::vtxLabel(29,-1,-1,-1,-1,false);
  result = hna1;
  
  //hna1 += ea1;
  giveLabels(ea1,hna1);

  if(!(result == hna1)){
    std::cout<<"UnitTest Error: half non-art += empty art not as expected: "<<hna1<<" != "<<result<<"\n";
    ierr++;
  }

//	half non-art += half art
  //same label
  hna1 = iceProp::vtxLabel(2,4,5,-1,-1,false);
  ha1 = iceProp::vtxLabel(10,4,3,-1,-1,true);
  result = hna1;

  //hna1 += ha1;
  giveLabels(ha1,hna1);

  if(!(result == hna1)){
    std::cout<<"UnitTest Error: half non-art += half art same labels not as expected: "<<hna1<<" != "<<result<<"\n";
    ierr++;
  }

  //ha1 sent hna1 its only label
  hna1 = iceProp::vtxLabel(2,4,5,-1,-1,false);
  ha1 = iceProp::vtxLabel(5,4,3,-1,-1,true);
  result = hna1;

  //hna1 += ha1;
  giveLabels(ha1,hna1);  

  if(!(result == hna1)){
    std::cout<<"UnitTest Error: half non-art += half art where art sent to non-art already not as expected: "<<hna1<<" != "<<result<<"\n";
    ierr++;
  }

  //different labels
  hna1 = iceProp::vtxLabel(2,3,4,-1,-1,false);
  ha1 = iceProp::vtxLabel(5,6,7,-1,-1,true);
  result = iceProp::vtxLabel(2,3,4,6,5,false);

  //hna1 += ha1;
  giveLabels(ha1,hna1);  

  if(!(result == hna1)){
    std::cout<<"UnitTest Error: half non-art += half art with different labels not as expected: "<<hna1<<" != "<<result<<"\n";
    ierr++;
  }

//	half non-art += full art
  //sharing a label
  hna1 = iceProp::vtxLabel(2,4,5,-1,-1,false);
  fa1 = iceProp::vtxLabel(6,4,3,9,0,true);
  result = iceProp::vtxLabel(2,4,5,6,6,false);

  //hna1 += fa1;
  giveLabels(fa1,hna1); 

  if(!(result == hna1)){
    std::cout<<"UnitTest Error: half non-art += full art sharing a label not as expected: "<<hna1<<" != "<< result<<"\n";
    ierr++;
  }

  //full sent a label to half
  hna1 = iceProp::vtxLabel(2,4,5,-1,-1,false);
  fa1 = iceProp::vtxLabel(5,4,3,6,7,true);
  result = hna1;

  //hna1 += fa1;
  giveLabels(fa1, hna1);

  if(!(hna1 == result)){
    std::cout<<"UnitTest Error: half non-art += full art full already sent a label not as expected: "<<hna1 <<" != "<<result<<"\n";
    ierr++;
  }

  //different labels
  hna1 = iceProp::vtxLabel(2,3,4,-1,-1,false);
  fa1 = iceProp::vtxLabel(1,5,6,7,8,true);
  result = iceProp::vtxLabel(2,3,4,1,1,false);

  //hna1 += fa1;
  giveLabels(fa1,hna1);

  if(!(result == hna1)){
    std::cout<<"UnitTest Error half non-art += full art different labels not as expected: "<<hna1<<" != "<<result<<"\n";
    ierr++;
  }

//	full non-art += empty non-art
  fna1 = iceProp::vtxLabel(2,3,4,5,6,false);
  ena1 = iceProp::vtxLabel(7);
  result = fna1;

  //fna1 += ena1;
  giveLabels(ena1, fna1);

  if(!(result == fna1)){
    std::cout<<"UnitTest Error full non-art += empty non-art not as expected: "<<fna1<<" != "<<result<<"\n";
    ierr++;
  }
  
//	full non-art += half non-art
  fna1 = iceProp::vtxLabel(2,3,4,5,6,false);
  hna1 = iceProp::vtxLabel(9,1,3,-1,-1,false);
  result = fna1;
  
  //fna1 += hna1;
  giveLabels(hna1, fna1);  

  if(!(result == fna1)){
    std::cout<<"UnitTest Error full non-art += half non-art not as expected: "<<fna1<<" != "<<result<<"\n";
    ierr++;
  } 


//	full non-art += full non-art
  fna1 = iceProp::vtxLabel(2,3,4,5,6,false);
  iceProp::vtxLabel fna2(6,7,8,9,10,false);
  result = fna1;

  //fna1 += fna2;
  giveLabels(fna2, fna1);

  if(!(result == fna1)){
    std::cout<<"UnitTest Error full non-art += full non-art not as expected: "<<fna1<<" != "<<result<<"\n";
    ierr++;
  }
 
//	full non-art += empty art
  fna1 = iceProp::vtxLabel(2,4,5,6,7,false);
  ea1 = iceProp::vtxLabel(1,-1,-1,-1,-1,true);
  result = fna1;
  
  //fna1 += ea1;
  giveLabels(ea1, fna1);

  if(!(result == fna1)){
    std::cout<<"UnitTest Error full non-art += empty art not as expected: "<<fna1<<" != "<<result<<"\n";
    ierr++;
  }

//	full non-art += half art
  fna1 = iceProp::vtxLabel(2,3,4,5,6,false);
  ha1 = iceProp::vtxLabel(1,10,11,-1,-1,true);
  result = fna1;

  //fna1 += ha1;
  giveLabels(ha1, fna1);

  if(!(result == fna1)){
    std::cout<<"UnitTest Error full non-art += half art not as expected: "<<fna1<<" != "<<result<<"\n";
    ierr++;
  }

//	full non-art += full art
  fna1 = iceProp::vtxLabel(2,3,4,5,6,false);
  fa1 = iceProp::vtxLabel(10,11,12,13,14,true);
  result = fna1;
  
  //fna1 += fa1;
  giveLabels(fa1,fna1);  

  if(!(result == fna1)) {
    std::cout<<"UnitTest Error full non-art += full art not as expected: "<<fna1<<" != "<<result<<"\n";
    ierr++;
  }
  while(!iceProp::reg.empty())iceProp::reg.pop();
  if(ierr == 0 ){
    std::cout<<"iceSheetPropagation::giveLabels OK\n";
  }
  return ierr;
}


#endif
