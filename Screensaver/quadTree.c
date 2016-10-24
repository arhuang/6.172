#include "./quadTree.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

QuadTree* QuadTree_new(const double left, const double right, const double down, const double up){
  QuadTree* quadTree = malloc(sizeof(QuadTree));
  if (quadTree==NULL) return NULL;
  quadTree->x1 = left;
  quadTree->x2 = right;
  quadTree->y1 = down;
  quadTree->y2 = up;

  quadTree->nwChild=NULL;
  quadTree->neChild=NULL;
  quadTree->swChild=NULL;
  quadTree->seChild=NULL;

  quadTree->lines = NULL;
  quadTree->last = NULL;
  quadTree->numLines=0;

  return quadTree;
}

void QuadTree_delete(QuadTree * quadTree){
  if(quadTree == NULL) return;
  QuadTree_delete(quadTree->nwChild);
  QuadTree_delete(quadTree->neChild);
  QuadTree_delete(quadTree->swChild);
  QuadTree_delete(quadTree->seChild);

  Node_freeNodes(quadTree->lines);
  free(quadTree);
}

void insertLine(Line*line, QuadTree* quadTree){
  Node* newnode = malloc(sizeof(Node));
  newnode->next = NULL;
  newnode->line = line;
  if (quadTree->last!=NULL) {
    quadTree->last->next=newnode;
  }
  quadTree->last = newnode;
  quadTree->lines = quadTree->lines == NULL ? newnode : quadTree->lines;
  quadTree->numLines++;
  return;
}

void checkCollision(QuadTree *quadTree, IntersectionEventList* events, CollisionWorld *collisionWorld){
  if(quadTree==NULL) return;
  
  Line* store[quadTree->numLines];
  int i=0;
  Node* node = quadTree->lines;
  while(node != NULL) {
    store[i] = node->line;
    node = node->next;
    i+=1;
  }
  for (int a=0; a<i; a++) {
    Line* l1 = store[a];
    for(int b=a+1; b<i; b++){
      Line* l2 = store[b];
      if(compareLines(l1,l2)>=0) {
        Line*temp = l1;
        l1 = l2;
        l2 = temp;
      }
      IntersectionType intersectionType = intersect(l1,l2,collisionWorld->timeStep);
      if (intersectionType != NO_INTERSECTION) {
        IntersectionEventList_appendNode(events, l1, l2, intersectionType);
        collisionWorld->numLineLineCollisions++;
      }
    }
  }

  for (int a = i-1; a >= 0; a--) {
    Line *line = store[a];
    collisionChild(line, quadTree->nwChild, events, collisionWorld);
    collisionChild(line, quadTree->neChild, events, collisionWorld);
    collisionChild(line, quadTree->swChild, events, collisionWorld);
    collisionChild(line, quadTree->seChild, events, collisionWorld);
  }

  if (quadTree->nwChild !=NULL) {
    checkCollision(quadTree->nwChild, events, collisionWorld);
    checkCollision(quadTree->neChild, events, collisionWorld);
    checkCollision(quadTree->swChild, events, collisionWorld);
    checkCollision(quadTree->seChild, events, collisionWorld);
  }
}

void collisionChild(Line* line, QuadTree* quadTree, IntersectionEventList* events, CollisionWorld *collisionWorld) {
  if (quadTree==NULL) return;
  
  Line* store[quadTree->numLines];
  int i=0;
  Node* node = quadTree->lines;
  while(node != NULL) {
    store[i] = node->line;
    node = node->next;
    i+=1;
  }
  assert(i==quadTree->numLines);
  for(int a=0; a<quadTree->numLines; a++) {
    Line* l1 = line, *l2 = store[a];
    if (compareLines(l1,l2)>=0){
      Line *temp = l1;
      l1 = l2;
      l2 = temp;
    }
    IntersectionType intersectionType = intersect(l1, l2, collisionWorld->timeStep);
    if (intersectionType != NO_INTERSECTION) {
      IntersectionEventList_appendNode(events, l1, l2, intersectionType);
      collisionWorld->numLineLineCollisions++;
    }
  }
  collisionChild(line, quadTree->nwChild, events, collisionWorld);
  collisionChild(line, quadTree->neChild, events, collisionWorld);
  collisionChild(line, quadTree->swChild, events, collisionWorld);
  collisionChild(line, quadTree->seChild, events, collisionWorld);
}

const int N = 50;

void putLineInChild(Line* line, QuadTree* quadTree, const double timestep){
  Vec p1 = line->p1, p2 = line->p2;
  Vec p3 = Vec_add(p1, Vec_multiply(line->velocity, timestep));
  Vec p4 = Vec_add(p2, Vec_multiply(line->velocity, timestep));
  if (parellelogramInNode(p1,p2,p3,p4,quadTree->nwChild)){
    insertLineChild(line,quadTree->nwChild,timestep);
    return;
  } else if (parellelogramInNode(p1,p2,p3,p4,quadTree->neChild)){
    insertLineChild(line,quadTree->neChild,timestep);
    return;
  } else if (parellelogramInNode(p1,p2,p3,p4,quadTree->swChild)){
    insertLineChild(line,quadTree->swChild,timestep);
    return;
  } else if (parellelogramInNode(p1,p2,p3,p4,quadTree->seChild)){
    insertLineChild(line,quadTree->seChild,timestep);
    return;
  } else {
    insertLine(line,quadTree);
  }
}

void insertLineChild(Line * line, QuadTree * quadTree, const double timestep) {

  if (quadTree->nwChild == NULL && quadTree->numLines < N) {
    insertLine(line, quadTree);
    return;
  } else if (quadTree->nwChild == NULL) {
    double midX = (quadTree->x1 + quadTree->x2)/2.;
    double midY = (quadTree->y1 + quadTree->y2)/2.;
    quadTree->nwChild = QuadTree_new(quadTree->x1,midX,midY,quadTree->y2);
    quadTree->neChild = QuadTree_new(midX,quadTree->x2,midY,quadTree->y2);
    quadTree->swChild = QuadTree_new(quadTree->x1,midX,quadTree->y1,midY);
    quadTree->seChild = QuadTree_new(midX,quadTree->x2,quadTree->y1,midY);
    
    Line* store[quadTree->numLines];
    int i=0;
    while(quadTree->lines!=NULL) {
      store[i] = quadTree->lines->line;
      quadTree->lines = quadTree->lines->next;
      quadTree->numLines-=1;
      i+=1;
    }
    quadTree->last = NULL;
    assert(quadTree->numLines==0);
    for (int n=0; n<i; n++) {
      putLineInChild(store[n],quadTree,timestep);
    }
  }

  putLineInChild(line,quadTree,timestep);
}

int sum=0;
void QuadTree_print(QuadTree*quadTree) {
  if (quadTree==NULL) return;
  sum+=quadTree->numLines;
  QuadTree_print(quadTree->nwChild);
  QuadTree_print(quadTree->neChild);
  QuadTree_print(quadTree->swChild);
  QuadTree_print(quadTree->seChild);
}

bool vecInNode(Vec vec,QuadTree* quadTree){
  return (vec.x < quadTree->x2 && vec.x > quadTree->x1 && vec.y < quadTree->y2 && vec.y > quadTree->y1);
}

bool parellelogramInNode(Vec p1, Vec p2, Vec p3, Vec p4, QuadTree* quadTree){
  return vecInNode(p1,quadTree) && vecInNode(p2,quadTree) && vecInNode(p3,quadTree) && vecInNode(p4,quadTree);
}
