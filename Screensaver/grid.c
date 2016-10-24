#include "./grid.h"
#include "./line.h"
#include "./intersection_detection.h"
#include <cilk/cilk.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <cilk/cilk.h>

Grid * Grid_new(double x1, double x2, double y1, double y2, int n) {
  Grid* grid = malloc(sizeof(Grid));
  grid->x1 = x1;
  grid->x2 = x2;
  grid->y1 = y1;
  grid->y2 = y2;
  grid->n = n;
  grid->width = (x2-x1)/n;
  grid->height = (y2-y1)/n;
  grid->cells = malloc(sizeof(List*)*n*n);
  for(int i=0; i<n*n; i++) {
    grid->cells[i] = List_create();
  }
  assert(grid->cells[0]->lines != NULL);
  return grid;
}

void Grid_free(Grid* grid){
  for(int i = 0; i < grid->n*grid->n; i++) {
    List_free(grid->cells[i]);
  }
}

List* get_cell(Grid* grid, double x, double y) {
  assert(x>=grid->x1);
  assert(y>=grid->y1);
  int a = (int)((x - grid->x1) / grid->width);
  int b = (int)((y - grid->y1) / grid->height);
  if (a<0) a=0;
  if (a>grid->n-1) a = grid->n-1;
  if (b<0) b=0;
  if (b>grid->n-1) b = grid->n-1;
  assert(grid->cells[a + (grid->n * b)]->lines != NULL);
  return grid->cells[a + (grid->n * b)];
}

void insert_line_in_cell(List* cell, Line* line) {
  assert(cell->lines != NULL);
  List_add(cell, line);
}

void insert_line(Line* line, Grid* grid) {
  for (double x = line->min_x; x < line->max_x + grid->width; x+=grid->width) {
    for (double y = line->min_y; y < line->max_y + grid->height; y+=grid->height) {
      assert(get_cell(grid,x,y) != NULL);
      assert(get_cell(grid, x, y)->lines != NULL);
      if (x <= grid->x2 && y <= grid->y2 && x >= grid->x1 && y >= grid->y1){
        insert_line_in_cell(get_cell(grid, x, y), line);
      }
    }
  }
}

void check_collision(Grid* grid, IntersectionEventList* events, CollisionWorld *collisionWorld) {
  int num = 64;
  IntersectionEventList temp_lists[num];
  int counts[num];
  for (int i = 0; i < num; i++) {
    temp_lists[i] = IntersectionEventList_make();
    counts[i] = 0;
  }
  cilk_for(int k = 0; k < num; k++) {
    assert(grid->n * grid->n % num = 0);
    for(int i=0; i<(grid->n*grid->n) / num; i++) {
      List* cell = grid->cells[i + ((k * grid->n*grid->n) / num)];
      for(int a=0; a<cell->current; a++){
        Line* l1 = cell->lines[a];
        for(int b=a+1; b<cell->current; b++){
          Line* l2 = cell->lines[b];
          if(compareLines(l1,l2)>=0) {
            Line*temp = l1;
            l1 = l2;
            l2 = temp;
          }
          IntersectionType intersectionType = intersect(l1,l2);
          if (intersectionType != NO_INTERSECTION) {
            IntersectionEventList_appendNode(&temp_lists[k], l1, l2, intersectionType);
            counts[k]++;
          }
        }
      }
    }
  }
  for (int i = 0; i < num; i++) {
    IntersectionEventNode* n = temp_lists[i].head;
    while (n != NULL) {
      IntersectionEventList_appendNode(events, n->l1, n->l2, n->intersectionType);
      n = n->next;
    }
    collisionWorld->numLineLineCollisions += counts[i];
  }
}
