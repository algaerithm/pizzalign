// Pizzalign scoring engine in C, compiled to WebAssembly.
// cells: N*N integers, -1 for empty, 0..7 for card types (same order as HTML TYPES).
static const int slices[8] = {0,1,3,5,6,5,3,1};
__attribute__((export_name("score_move")))
int score_move(const int *cells, int n, int pos, int card) {
  int r=pos/n, c=pos%n, pts=0;
  const int dr[4]={-1,1,0,0}, dc[4]={0,0,-1,1};
  for(int k=0;k<4;k++) {
    int rr=r+dr[k], cc=c+dc[k];
    if(rr<0||rr>=n||cc<0||cc>=n) continue;
    int other=cells[rr*n+cc]; if(other<0) continue;
    if(card==other) pts++;
    else if((card==0&&other==4)||(card==4&&other==0)||(card==2&&other==6)||(card==6&&other==2)) pts+=2;
  }
  // Count every 3-card straight window including the newly placed card.
  const int ar[2]={0,1}, ac[2]={1,0};
  for(int axis=0;axis<2;axis++) for(int off=-2;off<=0;off++) {
    int vals[3], valid=1, includes=0;
    for(int q=0;q<3;q++) {
      int rr=r+(off+q)*ar[axis], cc=c+(off+q)*ac[axis];
      if(rr<0||rr>=n||cc<0||cc>=n){valid=0;break;}
      int ix=rr*n+cc; if(ix==pos){vals[q]=card;includes=1;} else vals[q]=cells[ix];
      if(vals[q]<0){valid=0;break;}
    }
    if(valid&&includes){int a=slices[vals[0]],b=slices[vals[1]],d=slices[vals[2]];if((a<b&&b<d)||(a>b&&b>d))pts+=3;}
  }
  return pts;
}
