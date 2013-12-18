/////////////////////////////////////////////////
// Community detection algorithms
namespace TSnap {


namespace TSnapDetail {
// GIRVAN-NEWMAN algorithm
//	1. The betweenness of all existing edges in the network is calculated first.
//	2. The edge with the highest betweenness is removed.
//	3. The betweenness of all edges affected by the removal is recalculated.
//	4. Steps 2 and 3 are repeated until no edges remain.
//  Girvan M. and Newman M. E. J., Community structure in social and biological networks, Proc. Natl. Acad. Sci. USA 99, 7821-7826 (2002)
// Keep removing edges from Graph until one of the connected components of Graph splits into two.
void CmtyGirvanNewmanStep(PUNGraph& Graph, TIntV& Cmty1, TIntV& Cmty2) {
  TIntPrFltH BtwEH;
  TBreathFS<PUNGraph> BFS(Graph);
  Cmty1.Clr(false);  Cmty2.Clr(false);
  while (true) {
    TSnap::GetBetweennessCentr(Graph, BtwEH);
    BtwEH.SortByDat(false);
    if (BtwEH.Empty()) { return; }
    const int NId1 = BtwEH.GetKey(0).Val1;
    const int NId2 = BtwEH.GetKey(0).Val2;
    Graph->DelEdge(NId1, NId2);
    BFS.DoBfs(NId1, true, false, NId2, TInt::Mx);
    if (BFS.GetHops(NId1, NId2) == -1) { // two components
      TSnap::GetNodeWcc(Graph, NId1, Cmty1);
      TSnap::GetNodeWcc(Graph, NId2, Cmty2);
      return;
    }
  }
}

// Connected components of a graph define clusters
// OutDegH and OrigEdges stores node degrees and number of edges in the original graph
double _GirvanNewmanGetModularity(const PUNGraph& G, const TIntH& OutDegH, const int& OrigEdges, TCnComV& CnComV) {
  TSnap::GetWccs(G, CnComV); // get communities
  double Mod = 0;
  for (int c = 0; c < CnComV.Len(); c++) {
    const TIntV& NIdV = CnComV[c]();
    double EIn=0, EEIn=0;
    for (int i = 0; i < NIdV.Len(); i++) {
      TUNGraph::TNodeI NI = G->GetNI(NIdV[i]);
      EIn += NI.GetOutDeg();
      EEIn += OutDegH.GetDat(NIdV[i]);
    }
    Mod += (EIn-EEIn*EEIn/(2.0*OrigEdges));
  }
  if (Mod == 0) { return 0; }
  else { return Mod/(2.0*OrigEdges); }
}

TIntFltH MapEguationNew2Modules(PUNGraph& Graph, TIntH& module, TIntFltH& qi, int a, int b){
  TIntFltH qi1;
  qi1 = qi;	
  float inModule=0.0, outModule=0.0, val;
  int mds[2] = {a,b};
  for (int i=0; i<2; i++)
  {
    inModule=0.0, outModule=0.0;
    if (qi1.IsKey(mds[i])){
      int central_module = mds[i];
      for (TUNGraph::TEdgeI EI = Graph->BegEI(); EI < Graph->EndEI(); EI++){
        if (module.GetDat(EI.GetSrcNId()) == module.GetDat(EI.GetDstNId()) && module.GetDat(EI.GetDstNId()) == central_module)
          inModule += 1.0;
        else if ((module.GetDat(EI.GetSrcNId()) == central_module && module.GetDat(EI.GetDstNId()) != central_module) || (module.GetDat(EI.GetSrcNId()) != central_module && module.GetDat(EI.GetDstNId()) == central_module))
          outModule +=1.0;
	  }
      val = 0.0;
      if (inModule+outModule>0)
        val = outModule/(inModule+outModule);
      qi1.DelKey(mds[i]);
      qi1.AddDat(mds[i],val);
	}
	else{
		qi1.DelKey(mds[i]);
		qi1.AddDat(mds[i],0.0);
	}
  }
	
  return qi1;
}

float Equation(PUNGraph& Graph, TIntFltH& pAlpha,float& sumPAlphaLogPAlpha, TIntFltH& qi){
  float sumPAlpha = 1.0, sumQi = 0.0, sumQiLogQi=0.0, sumQiSumPAlphaLogQiSumPAlpha = 0.0;
  for (int i=0;i<qi.Len();i++){
    sumQi += qi[i];
    sumQiLogQi += qi[i]*log(qi[i]);
    sumQiSumPAlphaLogQiSumPAlpha += (qi[i]+sumPAlpha)*log(qi[i]+sumPAlpha);
  }
  return (sumQi*log(sumQi)-2*sumQiLogQi-sumPAlphaLogPAlpha+sumQiSumPAlphaLogQiSumPAlpha);
}

} // namespace TSnapDetail

// Maximum modularity clustering by Girvan-Newman algorithm (slow)
//  Girvan M. and Newman M. E. J., Community structure in social and biological networks, Proc. Natl. Acad. Sci. USA 99, 7821-7826 (2002)
double CommunityGirvanNewman(PUNGraph& Graph, TCnComV& CmtyV) {
  TIntH OutDegH;
  const int NEdges = Graph->GetEdges();
  for (TUNGraph::TNodeI NI = Graph->BegNI(); NI < Graph->EndNI(); NI++) {
    OutDegH.AddDat(NI.GetId(), NI.GetOutDeg());
  }
  double BestQ = -1; // modularity
  TCnComV CurCmtyV;
  CmtyV.Clr();
  TIntV Cmty1, Cmty2;
  while (true) {
    TSnapDetail::CmtyGirvanNewmanStep(Graph, Cmty1, Cmty2);
    const double Q = TSnapDetail::_GirvanNewmanGetModularity(Graph, OutDegH, NEdges, CurCmtyV);
    //printf("current modularity: %f\n", Q);
    if (Q > BestQ) {
      BestQ = Q; 
      CmtyV.Swap(CurCmtyV);
    }
    if (Cmty1.Len()==0 || Cmty2.Len() == 0) { break; }
  }
  return BestQ;
}

// Rosvall-Bergstrom community detection algorithm based on information theoretic approach.
// See: Rosvall M., Bergstrom C. T., Maps of random walks on complex networks reveal community structure, Proc. Natl. Acad. Sci. USA 105, 1118�1123 (2008)
double Infomap(PUNGraph& Graph, TCnComV& CmtyV){	
  TIntH DegH; 
  TIntFltH pAlpha; // probability of visiting node alpha
  TIntH module; // module of each node
  TIntFltH qi; // probaility of leaving each module
  float sumPAlphaLogPAlpha = 0.0;
  int br = 0;
  const int e = Graph->GetEdges(); 

  // initial values
  for (TUNGraph::TNodeI NI = Graph->BegNI(); NI < Graph->EndNI(); NI++) {
    DegH.AddDat(NI.GetId(), NI.GetDeg());
    float d = ((float)NI.GetDeg()/(float)(2*e));
    pAlpha.AddDat(NI.GetId(), d);
    sumPAlphaLogPAlpha += d*log(d);
    module.AddDat(NI.GetId(),br);
    qi.AddDat(module[br],1.0);
    br+=1;
  }

  float minCodeLength = TSnapDetail::Equation(Graph,pAlpha,sumPAlphaLogPAlpha,qi);
  float newCodeLength, prevIterationCodeLength = 0.0;
  int oldModule, newModule;

  do{
    prevIterationCodeLength = minCodeLength;
      for (TUNGraph::TNodeI NI = Graph->BegNI(); NI < Graph->EndNI(); NI++) {
        minCodeLength = TSnapDetail::Equation(Graph, pAlpha, sumPAlphaLogPAlpha, qi);
        for(int i=0; i<DegH.GetDat(NI.GetId()); i++){
          oldModule = module.GetDat(NI.GetId());
          newModule = module.GetDat(NI.GetNbrNId(i));
          if (oldModule!=newModule){
            module.DelKey(NI.GetId()); 
            module.AddDat(NI.GetId(),newModule);
            qi = TSnapDetail::MapEguationNew2Modules(Graph,module,qi,oldModule, newModule);
            newCodeLength = TSnapDetail::Equation(Graph,pAlpha,sumPAlphaLogPAlpha, qi);
            if (newCodeLength<minCodeLength){
              minCodeLength=newCodeLength;
              oldModule = newModule;
            }
            else{
              module.DelKey(NI.GetId());
              module.AddDat(NI.GetId(),oldModule);
            }
          }
       }
     }
   }while (minCodeLength<prevIterationCodeLength);

  module.SortByDat(true);
  int mod=-1;
  for (int i=0;i<module.Len();i++)
  {
    if (module[i]>mod){
      mod = module[i];
      TCnCom t;
      for (TUNGraph::TNodeI NI = Graph->BegNI(); NI < Graph->EndNI(); NI++){
        if (module.GetDat(NI.GetId())==mod)
        t.Add(NI.GetId());
      }
      CmtyV.Add(t);
    }
  }

  return minCodeLength;
}

namespace TSnapDetail {
/// Clauset-Newman-Moore community detection method.
/// At every step two communities that contribute maximum positive value to global modularity are merged.
/// See: Finding community structure in very large networks, A. Clauset, M.E.J. Newman, C. Moore, 2004
class TCNMQMatrix {
private:
  struct TCmtyDat {
    double DegFrac;
    TIntFltH NIdQH;
    int MxQId;
    TCmtyDat() : MxQId(-1) { }
    TCmtyDat(const double& NodeDegFrac, const int& OutDeg) : 
      DegFrac(NodeDegFrac), NIdQH(OutDeg), MxQId(-1) { }
    void AddQ(const int& NId, const double& Q) { NIdQH.AddDat(NId, Q);
      if (MxQId==-1 || NIdQH[MxQId]<Q) { MxQId=NIdQH.GetKeyId(NId); } }
    void UpdateMaxQ() { MxQId=-1; 
      for (int i = -1; NIdQH.FNextKeyId(i); ) { 
        if (MxQId==-1 || NIdQH[MxQId]< NIdQH[i]) { MxQId=i; } } }
    void DelLink(const int& K) { const int NId=GetMxQNId(); 
      NIdQH.DelKey(K); if (NId==K) { UpdateMaxQ(); }  }
    int GetMxQNId() const { return NIdQH.GetKey(MxQId); }
    double GetMxQ() const { return NIdQH[MxQId]; }
  };
private:
  THash<TInt, TCmtyDat> CmtyQH;
  THeap<TFltIntIntTr> MxQHeap;
  TUnionFind CmtyIdUF;
  double Q;
public:
  TCNMQMatrix(const PUNGraph& Graph) : CmtyQH(Graph->GetNodes()), 
    MxQHeap(Graph->GetNodes()), CmtyIdUF(Graph->GetNodes()) { Init(Graph); }
  void Init(const PUNGraph& Graph) {
    const double M = 0.5/Graph->GetEdges(); // 1/2m
    Q = 0.0;
    for (TUNGraph::TNodeI NI = Graph->BegNI(); NI < Graph->EndNI(); NI++) {
      CmtyIdUF.Add(NI.GetId());
      const int OutDeg = NI.GetOutDeg();
      if (OutDeg == 0) { continue; }
      TCmtyDat& Dat = CmtyQH.AddDat(NI.GetId(), TCmtyDat(M * OutDeg, OutDeg));
      for (int e = 0; e < NI.GetOutDeg(); e++) {
        const int DstNId = NI.GetOutNId(e);
        const double DstMod = 2 * M * (1.0 - OutDeg * Graph->GetNI(DstNId).GetOutDeg() * M);
        Dat.AddQ(DstNId, DstMod);
      }
      Q += -1.0*TMath::Sqr(OutDeg*M);
      if (NI.GetId() < Dat.GetMxQNId()) {
        MxQHeap.Add(TFltIntIntTr(Dat.GetMxQ(), NI.GetId(), Dat.GetMxQNId())); }
    }
    MxQHeap.MakeHeap();
  }
  TFltIntIntTr FindMxQEdge() {
    while (true) {
      if (MxQHeap.Empty()) { break; }
      const TFltIntIntTr TopQ = MxQHeap.PopHeap();
      if (! CmtyQH.IsKey(TopQ.Val2) || ! CmtyQH.IsKey(TopQ.Val3)) { continue; }
      if (TopQ.Val1!=CmtyQH.GetDat(TopQ.Val2).GetMxQ() && TopQ.Val1!=CmtyQH.GetDat(TopQ.Val3).GetMxQ()) { continue; }
      return TopQ;
    }
    return TFltIntIntTr(-1, -1, -1);
  }
  bool MergeBestQ() {
    const TFltIntIntTr TopQ = FindMxQEdge();
    if (TopQ.Val1 <= 0.0) { return false; }
    // joint communities
    const int I = TopQ.Val3;
    const int J = TopQ.Val2;
    CmtyIdUF.Union(I, J); // join
    Q += TopQ.Val1;
    TCmtyDat& DatJ = CmtyQH.GetDat(J);
    { TCmtyDat& DatI = CmtyQH.GetDat(I);
    DatI.DelLink(J);  DatJ.DelLink(I);
    for (int i = -1; DatJ.NIdQH.FNextKeyId(i); ) {
      const int K = DatJ.NIdQH.GetKey(i);
      TCmtyDat& DatK = CmtyQH.GetDat(K);
      double NewQ = DatJ.NIdQH[i];
      if (DatI.NIdQH.IsKey(K)) { NewQ = NewQ+DatI.NIdQH.GetDat(K);  DatK.DelLink(I); }     // K connected to I and J
      else { NewQ = NewQ-2*DatI.DegFrac*DatK.DegFrac; }  // K connected to J not I
      DatJ.AddQ(K, NewQ);
      DatK.AddQ(J, NewQ);
      MxQHeap.PushHeap(TFltIntIntTr(NewQ, TMath::Mn(J,K), TMath::Mx(J,K)));
    }
    for (int i = -1; DatI.NIdQH.FNextKeyId(i); ) {
      const int K = DatI.NIdQH.GetKey(i);
      if (! DatJ.NIdQH.IsKey(K)) { // K connected to I not J
        TCmtyDat& DatK = CmtyQH.GetDat(K);
        const double NewQ = DatI.NIdQH[i]-2*DatJ.DegFrac*DatK.DegFrac; 
        DatJ.AddQ(K, NewQ);
        DatK.DelLink(I);
        DatK.AddQ(J, NewQ);
        MxQHeap.PushHeap(TFltIntIntTr(NewQ, TMath::Mn(J,K), TMath::Mx(J,K)));
      }
    } 
    DatJ.DegFrac += DatI.DegFrac; }
    if (DatJ.NIdQH.Empty()) { CmtyQH.DelKey(J); } // isolated community (done)
    CmtyQH.DelKey(I);
    return true;
  }
  static double CmtyCMN(const PUNGraph& Graph, TCnComV& CmtyV) {
    TCNMQMatrix QMatrix(Graph);
    // maximize modularity
    while (QMatrix.MergeBestQ()) { }
    // reconstruct communities
    THash<TInt, TIntV> IdCmtyH;
    for (TUNGraph::TNodeI NI = Graph->BegNI(); NI < Graph->EndNI(); NI++) {
      IdCmtyH.AddDat(QMatrix.CmtyIdUF.Find(NI.GetId())).Add(NI.GetId()); 
    }
    CmtyV.Gen(IdCmtyH.Len());
    for (int j = 0; j < IdCmtyH.Len(); j++) {
      CmtyV[j].NIdV.Swap(IdCmtyH[j]);
    }
    return QMatrix.Q;
  }
};

} // namespace TSnapDetail

double CommunityCNM(const PUNGraph& Graph, TCnComV& CmtyV) {
  return TSnapDetail::TCNMQMatrix::CmtyCMN(Graph, CmtyV);
}

}; //namespace TSnap
