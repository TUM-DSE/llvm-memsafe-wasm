!RUN: %flang_fc1 -emit-hlfir -fopenmp %s -o - | FileCheck %s

!===============================================================================
! Target_Enter Simple
!===============================================================================

!CHECK-LABEL: func.func @_QPomp_target_enter_simple() {
subroutine omp_target_enter_simple
   integer :: a(1024)
   !CHECK: %[[BOUNDS:.*]] = omp.bounds   lower_bound({{.*}}) upper_bound({{.*}}) extent({{.*}}) stride({{.*}}) start_idx({{.*}})
   !CHECK: %[[MAP:.*]] = omp.map_info var_ptr({{.*}})   map_clauses(to) capture(ByRef) bounds(%[[BOUNDS]]) -> !fir.ref<!fir.array<1024xi32>> {name = "a"}
   !CHECK: omp.target_enter_data   map_entries(%[[MAP]] : !fir.ref<!fir.array<1024xi32>>)
   !$omp target enter data map(to: a)
    return
end subroutine omp_target_enter_simple

!===============================================================================
! Target_Enter Map types
!===============================================================================

!CHECK-LABEL: func.func @_QPomp_target_enter_mt() {
subroutine omp_target_enter_mt
   integer :: a(1024)
   integer :: b(1024)
   integer :: c(1024)
   integer :: d(1024)
   !CHECK: %[[BOUNDS_0:.*]] = omp.bounds   lower_bound({{.*}}) upper_bound({{.*}}) extent({{.*}}) stride({{.*}}) start_idx({{.*}})
   !CHECK: %[[MAP_0:.*]] = omp.map_info var_ptr({{.*}})   map_clauses(to) capture(ByRef) bounds(%[[BOUNDS_0]]) -> !fir.ref<!fir.array<1024xi32>> {name = "a"}
   !CHECK: %[[BOUNDS_1:.*]] = omp.bounds   lower_bound({{.*}}) upper_bound({{.*}}) extent({{.*}}) stride({{.*}}) start_idx({{.*}})
   !CHECK: %[[MAP_1:.*]] = omp.map_info var_ptr(%{{.*}})  map_clauses(to) capture(ByRef) bounds(%[[BOUNDS_1]]) -> !fir.ref<!fir.array<1024xi32>> {name = "b"}
   !CHECK: %[[BOUNDS_2:.*]] = omp.bounds   lower_bound({{.*}}) upper_bound({{.*}}) extent({{.*}}) stride({{.*}}) start_idx({{.*}})
   !CHECK: %[[MAP_2:.*]] = omp.map_info var_ptr({{.*}})   map_clauses(always, exit_release_or_enter_alloc) capture(ByRef) bounds(%[[BOUNDS_2]]) -> !fir.ref<!fir.array<1024xi32>> {name = "c"}
   !CHECK: %[[BOUNDS_3:.*]] = omp.bounds   lower_bound({{.*}}) upper_bound({{.*}}) extent({{.*}}) stride({{.*}}) start_idx({{.*}})
   !CHECK: %[[MAP_3:.*]] = omp.map_info var_ptr({{.*}})   map_clauses(to) capture(ByRef) bounds(%[[BOUNDS_3]]) -> !fir.ref<!fir.array<1024xi32>> {name = "d"}
   !CHECK: omp.target_enter_data   map_entries(%[[MAP_0]], %[[MAP_1]], %[[MAP_2]], %[[MAP_3]] : !fir.ref<!fir.array<1024xi32>>, !fir.ref<!fir.array<1024xi32>>, !fir.ref<!fir.array<1024xi32>>, !fir.ref<!fir.array<1024xi32>>)
   !$omp target enter data map(to: a, b) map(always, alloc: c) map(to: d)
end subroutine omp_target_enter_mt

!===============================================================================
! `Nowait` clause
!===============================================================================

!CHECK-LABEL: func.func @_QPomp_target_enter_nowait() {
subroutine omp_target_enter_nowait
   integer :: a(1024)
   !CHECK: %[[BOUNDS:.*]] = omp.bounds   lower_bound({{.*}}) upper_bound({{.*}}) extent({{.*}}) stride({{.*}}) start_idx({{.*}})
   !CHECK: %[[MAP:.*]] = omp.map_info var_ptr({{.*}})   map_clauses(to) capture(ByRef) bounds(%[[BOUNDS]]) -> !fir.ref<!fir.array<1024xi32>> {name = "a"}
   !CHECK: omp.target_enter_data  nowait map_entries(%[[MAP]] : !fir.ref<!fir.array<1024xi32>>)
   !$omp target enter data map(to: a) nowait
end subroutine omp_target_enter_nowait

!===============================================================================
! `if` clause
!===============================================================================

!CHECK-LABEL: func.func @_QPomp_target_enter_if() {
subroutine omp_target_enter_if
   integer :: a(1024)
   integer :: i
   i = 5
   !CHECK: %[[VAL_3:.*]] = fir.load %[[VAL_1:.*]] : !fir.ref<i32>
   !CHECK: %[[VAL_4:.*]] = arith.constant 10 : i32
   !CHECK: %[[VAL_5:.*]] = arith.cmpi slt, %[[VAL_3]], %[[VAL_4]] : i32
   !CHECK: %[[BOUNDS:.*]] = omp.bounds   lower_bound({{.*}}) upper_bound({{.*}}) extent({{.*}}) stride({{.*}}) start_idx({{.*}})
   !CHECK: %[[MAP:.*]] = omp.map_info var_ptr({{.*}})   map_clauses(to) capture(ByRef) bounds(%[[BOUNDS]]) -> !fir.ref<!fir.array<1024xi32>> {name = "a"}
   !CHECK: omp.target_enter_data   if(%[[VAL_5]] : i1) map_entries(%[[MAP]] : !fir.ref<!fir.array<1024xi32>>)
   !$omp target enter data if(i<10) map(to: a)
end subroutine omp_target_enter_if

!===============================================================================
! `device` clause
!===============================================================================

!CHECK-LABEL: func.func @_QPomp_target_enter_device() {
subroutine omp_target_enter_device
   integer :: a(1024)
   !CHECK: %[[VAL_1:.*]] = arith.constant 2 : i32
   !CHECK: %[[BOUNDS:.*]] = omp.bounds   lower_bound({{.*}}) upper_bound({{.*}}) extent({{.*}}) stride({{.*}}) start_idx({{.*}})
   !CHECK: %[[MAP:.*]] = omp.map_info var_ptr({{.*}})   map_clauses(to) capture(ByRef) bounds(%[[BOUNDS]]) -> !fir.ref<!fir.array<1024xi32>> {name = "a"}
   !CHECK: omp.target_enter_data   device(%[[VAL_1]] : i32) map_entries(%[[MAP]] : !fir.ref<!fir.array<1024xi32>>)
   !$omp target enter data map(to: a) device(2)
end subroutine omp_target_enter_device

!===============================================================================
! Target_Exit Simple
!===============================================================================

!CHECK-LABEL: func.func @_QPomp_target_exit_simple() {
subroutine omp_target_exit_simple
   integer :: a(1024)
   !CHECK: %[[BOUNDS:.*]] = omp.bounds   lower_bound({{.*}}) upper_bound({{.*}}) extent({{.*}}) stride({{.*}}) start_idx({{.*}})
   !CHECK: %[[MAP:.*]] = omp.map_info var_ptr({{.*}})   map_clauses(from) capture(ByRef) bounds(%[[BOUNDS]]) -> !fir.ref<!fir.array<1024xi32>> {name = "a"}
   !CHECK: omp.target_exit_data   map_entries(%[[MAP]] : !fir.ref<!fir.array<1024xi32>>)
   !$omp target exit data map(from: a)
end subroutine omp_target_exit_simple

!===============================================================================
! Target_Exit Map types
!===============================================================================

!CHECK-LABEL: func.func @_QPomp_target_exit_mt() {
subroutine omp_target_exit_mt
   integer :: a(1024)
   integer :: b(1024)
   integer :: c(1024)
   integer :: d(1024)
   integer :: e(1024)
   !CHECK: %[[BOUNDS_0:.*]] = omp.bounds   lower_bound({{.*}}) upper_bound({{.*}}) extent({{.*}}) stride({{.*}}) start_idx({{.*}})
   !CHECK: %[[MAP_0:.*]] = omp.map_info var_ptr({{.*}})   map_clauses(from) capture(ByRef) bounds(%[[BOUNDS_0]]) -> !fir.ref<!fir.array<1024xi32>> {name = "a"}
   !CHECK: %[[BOUNDS_1:.*]] = omp.bounds   lower_bound({{.*}}) upper_bound({{.*}}) extent({{.*}}) stride({{.*}}) start_idx({{.*}})
   !CHECK: %[[MAP_1:.*]] = omp.map_info var_ptr({{.*}})   map_clauses(from) capture(ByRef) bounds(%[[BOUNDS_1]]) -> !fir.ref<!fir.array<1024xi32>> {name = "b"}
   !CHECK: %[[BOUNDS_2:.*]] = omp.bounds   lower_bound({{.*}}) upper_bound({{.*}}) extent({{.*}}) stride({{.*}}) start_idx({{.*}})
   !CHECK: %[[MAP_2:.*]] = omp.map_info var_ptr({{.*}})   map_clauses(exit_release_or_enter_alloc) capture(ByRef) bounds(%[[BOUNDS_2]]) -> !fir.ref<!fir.array<1024xi32>> {name = "c"}
   !CHECK: %[[BOUNDS_3:.*]] = omp.bounds   lower_bound({{.*}}) upper_bound({{.*}}) extent({{.*}}) stride({{.*}}) start_idx({{.*}})
   !CHECK: %[[MAP_3:.*]] = omp.map_info var_ptr({{.*}})   map_clauses(always, delete) capture(ByRef) bounds(%[[BOUNDS_3]]) -> !fir.ref<!fir.array<1024xi32>> {name = "d"}
   !CHECK: %[[BOUNDS_4:.*]] = omp.bounds   lower_bound({{.*}}) upper_bound({{.*}}) extent({{.*}}) stride({{.*}}) start_idx({{.*}})
   !CHECK: %[[MAP_4:.*]] = omp.map_info var_ptr({{.*}})   map_clauses(from) capture(ByRef) bounds(%[[BOUNDS_4]]) -> !fir.ref<!fir.array<1024xi32>> {name = "e"}
   !CHECK: omp.target_exit_data map_entries(%[[MAP_0]], %[[MAP_1]], %[[MAP_2]], %[[MAP_3]], %[[MAP_4]] : !fir.ref<!fir.array<1024xi32>>, !fir.ref<!fir.array<1024xi32>>, !fir.ref<!fir.array<1024xi32>>, !fir.ref<!fir.array<1024xi32>>, !fir.ref<!fir.array<1024xi32>>)
   !$omp target exit data map(from: a,b) map(release: c) map(always, delete: d) map(from: e)
end subroutine omp_target_exit_mt

!===============================================================================
! `device` clause
!===============================================================================

!CHECK-LABEL: func.func @_QPomp_target_exit_device() {
subroutine omp_target_exit_device
   integer :: a(1024)
   integer :: d
   !CHECK: %[[VAL_2:.*]] = fir.load %[[VAL_1:.*]] : !fir.ref<i32>
   !CHECK: %[[BOUNDS:.*]] = omp.bounds   lower_bound({{.*}}) upper_bound({{.*}}) extent({{.*}}) stride({{.*}}) start_idx({{.*}})
   !CHECK: %[[MAP:.*]] = omp.map_info var_ptr({{.*}})   map_clauses(from) capture(ByRef) bounds(%[[BOUNDS]]) -> !fir.ref<!fir.array<1024xi32>> {name = "a"}
   !CHECK: omp.target_exit_data   device(%[[VAL_2]] : i32) map_entries(%[[MAP]] : !fir.ref<!fir.array<1024xi32>>)
   !$omp target exit data map(from: a) device(d)
end subroutine omp_target_exit_device

!===============================================================================
! Target_Data with region
!===============================================================================

!CHECK-LABEL: func.func @_QPomp_target_data() {
subroutine omp_target_data
   !CHECK: %[[VAL_0:.*]] = fir.alloca !fir.array<1024xi32> {bindc_name = "a", uniq_name = "_QFomp_target_dataEa"}
   !CHECK: %[[A_DECL:.*]]:2 = hlfir.declare %[[VAL_0]](%{{.*}}) {uniq_name = "_QFomp_target_dataEa"} : (!fir.ref<!fir.array<1024xi32>>, !fir.shape<1>) -> (!fir.ref<!fir.array<1024xi32>>, !fir.ref<!fir.array<1024xi32>>)
   integer :: a(1024)
   !CHECK: %[[BOUNDS:.*]] = omp.bounds   lower_bound({{.*}}) upper_bound({{.*}}) extent({{.*}}) stride({{.*}}) start_idx({{.*}})
   !CHECK: %[[MAP:.*]] = omp.map_info var_ptr(%[[A_DECL]]#1 : !fir.ref<!fir.array<1024xi32>>, !fir.array<1024xi32>)   map_clauses(tofrom) capture(ByRef) bounds(%[[BOUNDS]]) -> !fir.ref<!fir.array<1024xi32>> {name = "a"}
   !CHECK: omp.target_data   map_entries(%[[MAP]] : !fir.ref<!fir.array<1024xi32>>) {
   !$omp target data map(tofrom: a)
      !CHECK: %[[C10:.*]] = arith.constant 10 : i32
      !CHECK: %[[C1:.*]] = arith.constant 1 : index
      !CHECK: %[[A_1:.*]] = hlfir.designate %[[A_DECL]]#0 (%[[C1]])  : (!fir.ref<!fir.array<1024xi32>>, index) -> !fir.ref<i32>
      !CHECK: hlfir.assign %[[C10]] to %[[A_1]] : i32, !fir.ref<i32
      a(1) = 10
   !CHECK: omp.terminator
   !$omp end target data
   !CHECK: }
end subroutine omp_target_data

!CHECK-LABEL: func.func @_QPomp_target_data_mt
subroutine omp_target_data_mt
   integer :: a(1024)
   integer :: b(1024)
   !CHECK: %[[VAR_A:.*]] = fir.alloca !fir.array<1024xi32> {bindc_name = "a", uniq_name = "_QFomp_target_data_mtEa"}
   !CHECK: %[[VAR_A_DECL:.*]]:2 = hlfir.declare %[[VAR_A]](%{{.*}}) {uniq_name = "_QFomp_target_data_mtEa"} : (!fir.ref<!fir.array<1024xi32>>, !fir.shape<1>) -> (!fir.ref<!fir.array<1024xi32>>, !fir.ref<!fir.array<1024xi32>>)
   !CHECK: %[[VAR_B:.*]] = fir.alloca !fir.array<1024xi32> {bindc_name = "b", uniq_name = "_QFomp_target_data_mtEb"}
   !CHECK: %[[VAR_B_DECL:.*]]:2 = hlfir.declare %[[VAR_B]](%{{.*}}) {uniq_name = "_QFomp_target_data_mtEb"} : (!fir.ref<!fir.array<1024xi32>>, !fir.shape<1>) -> (!fir.ref<!fir.array<1024xi32>>, !fir.ref<!fir.array<1024xi32>>)
   !CHECK: %[[BOUNDS_A:.*]] = omp.bounds   lower_bound({{.*}}) upper_bound({{.*}}) extent({{.*}}) stride({{.*}}) start_idx({{.*}})
   !CHECK: %[[MAP_A:.*]] = omp.map_info var_ptr(%[[VAR_A_DECL]]#1 : !fir.ref<!fir.array<1024xi32>>, !fir.array<1024xi32>)   map_clauses(tofrom) capture(ByRef) bounds(%[[BOUNDS_A]]) -> !fir.ref<!fir.array<1024xi32>> {name = "a"}
   !CHECK: omp.target_data   map_entries(%[[MAP_A]] : !fir.ref<!fir.array<1024xi32>>) {
   !$omp target data map(a)
   !CHECK: omp.terminator
   !$omp end target data
   !CHECK: }
   !CHECK: %[[BOUNDS_B:.*]] = omp.bounds   lower_bound({{.*}}) upper_bound({{.*}}) extent({{.*}}) stride({{.*}}) start_idx({{.*}})
   !CHECK: %[[MAP_B:.*]] = omp.map_info var_ptr(%[[VAR_B_DECL]]#1 : !fir.ref<!fir.array<1024xi32>>, !fir.array<1024xi32>)   map_clauses(always, from) capture(ByRef) bounds(%[[BOUNDS_B]]) -> !fir.ref<!fir.array<1024xi32>> {name = "b"}
   !CHECK: omp.target_data   map_entries(%[[MAP_B]] : !fir.ref<!fir.array<1024xi32>>) {
   !$omp target data map(always, from : b)
   !CHECK: omp.terminator
   !$omp end target data
   !CHECK: }
end subroutine omp_target_data_mt

!===============================================================================
! Target with region
!===============================================================================

!CHECK-LABEL: func.func @_QPomp_target() {
subroutine omp_target
   !CHECK: %[[VAL_1:.*]]:2 = hlfir.declare %{{.*}}(%{{.*}}) {uniq_name = "_QFomp_targetEa"} : (!fir.ref<!fir.array<1024xi32>>, !fir.shape<1>) -> (!fir.ref<!fir.array<1024xi32>>, !fir.ref<!fir.array<1024xi32>>)
   integer :: a(1024)
   !CHECK: %[[BOUNDS:.*]] = omp.bounds   lower_bound({{.*}}) upper_bound({{.*}}) extent({{.*}}) stride({{.*}}) start_idx({{.*}})
   !CHECK: %[[MAP:.*]] = omp.map_info var_ptr(%[[VAL_1]]#1 : !fir.ref<!fir.array<1024xi32>>, !fir.array<1024xi32>) map_clauses(tofrom) capture(ByRef) bounds(%[[BOUNDS]]) -> !fir.ref<!fir.array<1024xi32>> {name = "a"}
   !CHECK: omp.target   map_entries(%[[MAP]] -> %[[ARG_0:.*]], %{{.*}} -> %[[ARG_1:.*]] : !fir.ref<!fir.array<1024xi32>>, index) {
   !CHECK: ^bb0(%[[ARG_0]]: !fir.ref<!fir.array<1024xi32>>, %[[ARG_1]]: index):
   !$omp target map(tofrom: a)
      !CHECK: %[[VAL_2:.*]] = fir.shape %[[ARG_1]] : (index) -> !fir.shape<1>
      !CHECK: %[[VAL_3:.*]]:2 = hlfir.declare %[[ARG_0]](%[[VAL_2]]) {uniq_name = "_QFomp_targetEa"} : (!fir.ref<!fir.array<1024xi32>>, !fir.shape<1>) -> (!fir.ref<!fir.array<1024xi32>>, !fir.ref<!fir.array<1024xi32>>)
      !CHECK: %[[VAL_4:.*]] = arith.constant 10 : i32
      !CHECK: %[[VAL_5:.*]] = arith.constant 1 : index
      !CHECK: %[[VAL_6:.*]] = hlfir.designate %[[VAL_3]]#0 (%[[VAL_5]])  : (!fir.ref<!fir.array<1024xi32>>, index) -> !fir.ref<i32>
      !CHECK: hlfir.assign %[[VAL_4]] to %[[VAL_6]] : i32, !fir.ref<i32>
      a(1) = 10
      !CHECK: omp.terminator
   !$omp end target
   !CHECK: }
end subroutine omp_target

!===============================================================================
! Target implicit capture
!===============================================================================

!CHECK-LABEL: func.func @_QPomp_target_implicit() {
subroutine omp_target_implicit
   !CHECK: %[[VAL_0:.*]] = arith.constant 1024 : index
   !CHECK: %[[VAL_1:.*]] = fir.alloca !fir.array<1024xi32> {bindc_name = "a", uniq_name = "_QFomp_target_implicitEa"}
   !CHECK: %[[VAL_2:.*]] = fir.shape %[[VAL_0]] : (index) -> !fir.shape<1>
   !CHECK: %[[VAL_3:.*]]:2 = hlfir.declare %[[VAL_1]](%[[VAL_2]]) {uniq_name = "_QFomp_target_implicitEa"} : (!fir.ref<!fir.array<1024xi32>>, !fir.shape<1>) -> (!fir.ref<!fir.array<1024xi32>>, !fir.ref<!fir.array<1024xi32>>)
   integer :: a(1024)
   !CHECK: %[[VAL_4:.*]] = omp.map_info var_ptr(%[[VAL_3]]#1 : !fir.ref<!fir.array<1024xi32>>, !fir.array<1024xi32>)   map_clauses(implicit, tofrom) capture(ByRef) bounds(%{{.*}}) -> !fir.ref<!fir.array<1024xi32>> {name = "a"}
   !CHECK: %[[VAL_5:.*]] = omp.map_info val(%[[VAL_0]] : index)   map_clauses(implicit, exit_release_or_enter_alloc) capture(ByCopy) -> index {name = ""}
   !CHECK: omp.target   map_entries(%[[VAL_4]] -> %[[VAL_6:.*]], %[[VAL_5]] -> %[[VAL_7:.*]] : !fir.ref<!fir.array<1024xi32>>, index) {
   !CHECK: ^bb0(%[[VAL_6]]: !fir.ref<!fir.array<1024xi32>>, %[[VAL_7]]: index):
   !$omp target
      !CHECK: %[[VAL_8:.*]] = fir.shape %[[VAL_7]] : (index) -> !fir.shape<1>
      !CHECK: %[[VAL_9:.*]]:2 = hlfir.declare %[[VAL_6]](%[[VAL_8]]) {uniq_name = "_QFomp_target_implicitEa"} : (!fir.ref<!fir.array<1024xi32>>, !fir.shape<1>) -> (!fir.ref<!fir.array<1024xi32>>, !fir.ref<!fir.array<1024xi32>>)
      !CHECK: %[[VAL_10:.*]] = arith.constant 10 : i32
      !CHECK: %[[VAL_11:.*]] = arith.constant 1 : index
      !CHECK: %[[VAL_12:.*]] = hlfir.designate %[[VAL_9]]#0 (%[[VAL_11]])  : (!fir.ref<!fir.array<1024xi32>>, index) -> !fir.ref<i32>
      !CHECK: hlfir.assign %[[VAL_10]] to %[[VAL_12]] : i32, !fir.ref<i32>
      a(1) = 10
      !CHECK: omp.terminator
   !$omp end target
   !CHECK: }
end subroutine omp_target_implicit

!===============================================================================
! Target implicit capture nested
!===============================================================================

!CHECK-LABEL: func.func @_QPomp_target_implicit_nested() {
subroutine omp_target_implicit_nested
   integer::a, b
   !CHECK: omp.target   map_entries(%{{.*}} -> %[[ARG0:.*]], %{{.*}} -> %[[ARG1:.*]] : !fir.ref<i32>, !fir.ref<i32>) {
   !CHECK: ^bb0(%[[ARG0]]: !fir.ref<i32>, %[[ARG1]]: !fir.ref<i32>):
   !$omp target
      !CHECK: %[[VAL_8:.*]]:2 = hlfir.declare %[[ARG0]] {uniq_name = "_QFomp_target_implicit_nestedEa"} : (!fir.ref<i32>) -> (!fir.ref<i32>, !fir.ref<i32>)
      !CHECK: %[[VAL_9:.*]]:2 = hlfir.declare %[[ARG1]] {uniq_name = "_QFomp_target_implicit_nestedEb"} : (!fir.ref<i32>) -> (!fir.ref<i32>, !fir.ref<i32>)
      !CHECK: %[[VAL_10:.*]] = arith.constant 10 : i32
      !CHECK: hlfir.assign %[[VAL_10]] to %[[VAL_8]]#0 : i32, !fir.ref<i32>
      a = 10
      !CHECK: omp.parallel
      !$omp parallel
         !CHECK: %[[VAL_11:.*]] = arith.constant 20 : i32
         !CHECK: hlfir.assign %[[VAL_11]] to %[[VAL_9]]#0 : i32, !fir.ref<i32>
         b = 20
         !CHECK: omp.terminator
      !$omp end parallel
      !CHECK: omp.terminator
   !$omp end target
   !CHECK: }
end subroutine omp_target_implicit_nested

!===============================================================================
! Target implicit capture with bounds
!===============================================================================

!CHECK-LABEL: func.func @_QPomp_target_implicit_bounds(%{{.*}}: !fir.ref<i32> {fir.bindc_name = "n"}) {
subroutine omp_target_implicit_bounds(n)
   !CHECK: %[[VAL_1:.*]] = arith.select %{{.*}}, %{{.*}}, %{{.*}} : index
   !CHECK: %[[VAL_2:.*]] = arith.select %{{.*}}, %{{.*}}, %{{.*}} : index
   !CHECK: %[[VAL_3:.*]] = fir.alloca !fir.array<?x1024xi32>, %[[VAL_1]] {bindc_name = "a", uniq_name = "_QFomp_target_implicit_boundsEa"}
   !CHECK: %[[VAL_4:.*]] = fir.shape %[[VAL_1]], %[[VAL_2]] : (index, index) -> !fir.shape<2>
   !CHECK: %[[VAL_5:.*]]:2 = hlfir.declare %[[VAL_3]](%[[VAL_4]]) {uniq_name = "_QFomp_target_implicit_boundsEa"} : (!fir.ref<!fir.array<?x1024xi32>>, !fir.shape<2>) -> (!fir.box<!fir.array<?x1024xi32>>, !fir.ref<!fir.array<?x1024xi32>>)
   integer :: n
   integer :: a(n, 1024)
   !CHECK: %[[VAL_6:.*]] = omp.map_info var_ptr(%[[VAL_5]]#1 : !fir.ref<!fir.array<?x1024xi32>>, !fir.array<?x1024xi32>)   map_clauses(implicit, tofrom) capture(ByRef) bounds(%{{.*}}) -> !fir.ref<!fir.array<?x1024xi32>> {name = "a"}
   !CHECK: %[[VAL_7:.*]] = omp.map_info val(%[[VAL_1]] : index)   map_clauses(implicit, exit_release_or_enter_alloc) capture(ByCopy) -> index {name = ""}
   !CHECK: %[[VAL_8:.*]] = omp.map_info val(%[[VAL_2]] : index)   map_clauses(implicit, exit_release_or_enter_alloc) capture(ByCopy) -> index {name = ""}
   !CHECK: omp.target   map_entries(%[[VAL_6]] -> %[[ARG_1:.*]], %[[VAL_7]] -> %[[ARG_2:.*]], %[[VAL_8]] -> %[[ARG_3:.*]] : !fir.ref<!fir.array<?x1024xi32>>, index, index) {
   !CHECK: ^bb0(%[[ARG_1]]: !fir.ref<!fir.array<?x1024xi32>>, %[[ARG_2]]: index, %[[ARG_3]]: index):
   !$omp target
      !CHECK: %[[VAL_9:.*]] = fir.shape %[[ARG_2]], %[[ARG_3]] : (index, index) -> !fir.shape<2>
      !CHECK: %[[VAL_10:.*]]:2 = hlfir.declare %[[ARG_1]](%[[VAL_9]]) {uniq_name = "_QFomp_target_implicit_boundsEa"} : (!fir.ref<!fir.array<?x1024xi32>>, !fir.shape<2>) -> (!fir.box<!fir.array<?x1024xi32>>, !fir.ref<!fir.array<?x1024xi32>>)
      !CHECK: %[[VAL_11:.*]] = arith.constant 33 : i32
      !CHECK: %[[VAL_12:.*]] = arith.constant 11 : index
      !CHECK: %[[VAL_13:.*]] = arith.constant 22 : index
      !CHECK: %[[VAL_14:.*]] = hlfir.designate %[[VAL_10]]#0 (%[[VAL_12]], %[[VAL_13]])  : (!fir.box<!fir.array<?x1024xi32>>, index, index) -> !fir.ref<i32>
      !CHECK: hlfir.assign %[[VAL_11]] to %[[VAL_14]] : i32, !fir.ref<i32>
      a(11, 22) = 33
      !CHECK: omp.terminator
   !$omp end target
!CHECK: }
end subroutine omp_target_implicit_bounds

!===============================================================================
! Target `thread_limit` clause
!===============================================================================

!CHECK-LABEL: func.func @_QPomp_target_thread_limit() {
subroutine omp_target_thread_limit
   integer :: a
   !CHECK: %[[VAL_1:.*]] = arith.constant 64 : i32
   !CHECK: %[[MAP:.*]] = omp.map_info var_ptr({{.*}})   map_clauses(tofrom) capture(ByRef) -> !fir.ref<i32> {name = "a"}
   !CHECK: omp.target   thread_limit(%[[VAL_1]] : i32) map_entries(%[[MAP]] -> %{{.*}} : !fir.ref<i32>) {
   !CHECK: ^bb0(%{{.*}}: !fir.ref<i32>):
   !$omp target map(tofrom: a) thread_limit(64)
      a = 10
   !CHECK: omp.terminator
   !$omp end target
   !CHECK: }
end subroutine omp_target_thread_limit

!===============================================================================
! Target `use_device_ptr` clause
!===============================================================================

!CHECK-LABEL: func.func @_QPomp_target_device_ptr() {
subroutine omp_target_device_ptr
   use iso_c_binding, only : c_ptr, c_loc
   type(c_ptr) :: a
   integer, target :: b
   !CHECK: %[[MAP:.*]] = omp.map_info var_ptr({{.*}})   map_clauses(tofrom) capture(ByRef) -> {{.*}} {name = "a"}
   !CHECK: omp.target_data map_entries(%[[MAP]]{{.*}}
   !$omp target data map(tofrom: a) use_device_ptr(a)
   !CHECK: ^bb0(%[[VAL_1:.*]]: !fir.ref<!fir.type<_QM__fortran_builtinsT__builtin_c_ptr{__address:i64}>>):
   !CHECK: {{.*}} = fir.coordinate_of %[[VAL_1:.*]], {{.*}} : (!fir.ref<!fir.type<_QM__fortran_builtinsT__builtin_c_ptr{__address:i64}>>, !fir.field) -> !fir.ref<i64>
      a = c_loc(b)
   !CHECK: omp.terminator
   !$omp end target data
   !CHECK: }
end subroutine omp_target_device_ptr

 !===============================================================================
 ! Target `use_device_addr` clause
 !===============================================================================

 !CHECK-LABEL: func.func @_QPomp_target_device_addr() {
 subroutine omp_target_device_addr
   integer, pointer :: a
   !CHECK: %[[VAL_0:.*]] = fir.alloca !fir.box<!fir.ptr<i32>> {bindc_name = "a", uniq_name = "_QFomp_target_device_addrEa"}
   !CHECK: %[[VAL_0_DECL:.*]]:2 = hlfir.declare %0 {fortran_attrs = #fir.var_attrs<pointer>, uniq_name = "_QFomp_target_device_addrEa"} : (!fir.ref<!fir.box<!fir.ptr<i32>>>) -> (!fir.ref<!fir.box<!fir.ptr<i32>>>, !fir.ref<!fir.box<!fir.ptr<i32>>>)
   !CHECK: %[[MAP:.*]] = omp.map_info var_ptr({{.*}})   map_clauses(tofrom) capture(ByRef) -> {{.*}} {name = "a"}
   !CHECK: omp.target_data map_entries(%[[MAP]] : {{.*}}) use_device_addr(%[[VAL_0_DECL]]#1 : !fir.ref<!fir.box<!fir.ptr<i32>>>) {
   !$omp target data map(tofrom: a) use_device_addr(a)
   !CHECK: ^bb0(%[[VAL_1:.*]]: !fir.ref<!fir.box<!fir.ptr<i32>>>):
   !CHECK: %[[VAL_1_DECL:.*]]:2 = hlfir.declare %[[VAL_1]] {fortran_attrs = #fir.var_attrs<pointer>, uniq_name = "_QFomp_target_device_addrEa"} : (!fir.ref<!fir.box<!fir.ptr<i32>>>) -> (!fir.ref<!fir.box<!fir.ptr<i32>>>, !fir.ref<!fir.box<!fir.ptr<i32>>>)
   !CHECK: %[[C10:.*]] = arith.constant 10 : i32
   !CHECK: %[[A_BOX:.*]] = fir.load %[[VAL_1_DECL]]#0 : !fir.ref<!fir.box<!fir.ptr<i32>>>
   !CHECK: %[[A_ADDR:.*]] = fir.box_addr %[[A_BOX]] : (!fir.box<!fir.ptr<i32>>) -> !fir.ptr<i32>
   !CHECK: hlfir.assign %[[C10]] to %[[A_ADDR]] : i32, !fir.ptr<i32>
      a = 10
   !CHECK: omp.terminator
   !$omp end target data
   !CHECK: }
end subroutine omp_target_device_addr


!===============================================================================
! Target Data with unstructured code
!===============================================================================
!CHECK-LABEL: func.func @_QPsb
subroutine sb
   integer :: i = 1
   integer :: j = 11
!CHECK: omp.target_data map_entries(%{{.*}}, %{{.*}} : !fir.ref<i32>, !fir.ref<i32>)
   !$omp target data map(tofrom: i, j)
     j = j - 1
!CHECK: %[[J_VAL:.*]] = arith.subi
!CHECK: hlfir.assign %[[J_VAL]]
!CHECK: cf.br ^[[BB:.*]]
!CHECK: ^[[BB]]:
     goto 20
20   i = i + 1
!CHECK: %[[I_VAL:.*]] = arith.addi
!CHECK: hlfir.assign %[[I_VAL]]
!CHECK: omp.terminator
   !$omp end target data
end subroutine

!===============================================================================
! Target with parallel loop
!===============================================================================

!CHECK-LABEL: func.func @_QPomp_target_parallel_do() {
subroutine omp_target_parallel_do
   !CHECK: %[[C1024:.*]] = arith.constant 1024 : index
   !CHECK: %[[VAL_0:.*]] = fir.alloca !fir.array<1024xi32> {bindc_name = "a", uniq_name = "_QFomp_target_parallel_doEa"}
   !CHECK: %[[VAL_0_DECL:.*]]:2 = hlfir.declare %[[VAL_0]](%{{.*}}) {uniq_name = "_QFomp_target_parallel_doEa"} : (!fir.ref<!fir.array<1024xi32>>, !fir.shape<1>) -> (!fir.ref<!fir.array<1024xi32>>, !fir.ref<!fir.array<1024xi32>>)
   integer :: a(1024)
   !CHECK: %[[VAL_1:.*]] = fir.alloca i32 {bindc_name = "i", uniq_name = "_QFomp_target_parallel_doEi"}
   !CHECK: %[[VAL_1_DECL:.*]]:2 = hlfir.declare %[[VAL_1]] {uniq_name = "_QFomp_target_parallel_doEi"} : (!fir.ref<i32>) -> (!fir.ref<i32>, !fir.ref<i32>)
   integer :: i
   !CHECK: %[[C1:.*]] = arith.constant 1 : index
   !CHECK: %[[C0:.*]] = arith.constant 0 : index
   !CHECK: %[[SUB:.*]] = arith.subi %[[C1024]], %[[C1]] : index
   !CHECK: %[[BOUNDS:.*]] = omp.bounds   lower_bound(%[[C0]] : index) upper_bound(%[[SUB]] : index) extent(%[[C1024]] : index) stride(%[[C1]] : index) start_idx(%[[C1]] : index)
   !CHECK: %[[MAP:.*]] = omp.map_info var_ptr(%[[VAL_0_DECL]]#1 : !fir.ref<!fir.array<1024xi32>>, !fir.array<1024xi32>)   map_clauses(tofrom) capture(ByRef) bounds(%[[BOUNDS]]) -> !fir.ref<!fir.array<1024xi32>> {name = "a"}
   !CHECK: omp.target   map_entries(%[[MAP]] -> %[[ARG_0:.*]], %{{.*}} -> %{{.*}}, %{{.*}} -> %{{.*}} : !fir.ref<!fir.array<1024xi32>>, !fir.ref<i32>, index) {
   !CHECK: ^bb0(%[[ARG_0]]: !fir.ref<!fir.array<1024xi32>>, %{{.*}}: !fir.ref<i32>, %{{.*}}: index):
      !CHECK: %[[VAL_0_DECL:.*]]:2 = hlfir.declare %[[ARG_0]](%{{.*}}) {uniq_name = "_QFomp_target_parallel_doEa"} : (!fir.ref<!fir.array<1024xi32>>, !fir.shape<1>) -> (!fir.ref<!fir.array<1024xi32>>, !fir.ref<!fir.array<1024xi32>>)
      !CHECK: omp.parallel
      !$omp target parallel do map(tofrom: a)
         !CHECK: %[[I_PVT_ALLOCA:.*]] = fir.alloca i32 {adapt.valuebyref, pinned}
         !CHECK: %[[I_PVT_DECL:.*]]:2 = hlfir.declare %[[I_PVT_ALLOCA]] {uniq_name = "_QFomp_target_parallel_doEi"} : (!fir.ref<i32>) -> (!fir.ref<i32>, !fir.ref<i32>)
         !CHECK: omp.wsloop for  (%[[I_VAL:.*]]) : i32
         do i = 1, 1024
           !CHECK:   fir.store %[[I_VAL]] to %[[I_PVT_DECL]]#1 : !fir.ref<i32>
           !CHECK:   %[[C10:.*]] = arith.constant 10 : i32
           !CHECK:   %[[I_PVT_VAL:.*]] = fir.load %[[I_PVT_DECL]]#0 : !fir.ref<i32>
           !CHECK:   %[[I_VAL:.*]] = fir.convert %[[I_PVT_VAL]] : (i32) -> i64
           !CHECK:   %[[A_I:.*]] = hlfir.designate %[[VAL_0_DECL]]#0 (%[[I_VAL]])  : (!fir.ref<!fir.array<1024xi32>>, i64) -> !fir.ref<i32>
           !CHECK:   hlfir.assign %[[C10]] to %[[A_I]] : i32, !fir.ref<i32>
            a(i) = 10
         end do
         !CHECK: omp.yield
         !CHECK: }
      !CHECK: omp.terminator
      !CHECK: }
   !CHECK: omp.terminator
   !CHECK: }
   !$omp end target parallel do
end subroutine omp_target_parallel_do

!===============================================================================
! Target with unstructured code
!===============================================================================

!CHECK-LABEL:   func.func @_QPtarget_unstructured() {
subroutine target_unstructured
   !CHECK: %[[VAL_1:.*]]:2 = hlfir.declare %{{.*}} {uniq_name = "_QFtarget_unstructuredEi"} : (!fir.ref<i32>) -> (!fir.ref<i32>, !fir.ref<i32>)
   integer :: i = 1
   !CHECK: %[[VAL_3:.*]]:2 = hlfir.declare %{{.*}} {uniq_name = "_QFtarget_unstructuredEj"} : (!fir.ref<i32>) -> (!fir.ref<i32>, !fir.ref<i32>)
   integer :: j = 11
   !CHECK: %[[VAL_4:.*]] = omp.map_info var_ptr(%[[VAL_1]]#1 : !fir.ref<i32>, i32) map_clauses(implicit, exit_release_or_enter_alloc) capture(ByCopy) -> !fir.ref<i32> {name = "i"}
   !CHECK: %[[VAL_5:.*]] = omp.map_info var_ptr(%[[VAL_3]]#1 : !fir.ref<i32>, i32) map_clauses(implicit, exit_release_or_enter_alloc) capture(ByCopy) -> !fir.ref<i32> {name = "j"}
   !CHECK: omp.target map_entries(%[[VAL_4]] -> %[[VAL_6:.*]], %[[VAL_5]] -> %[[VAL_7:.*]] : !fir.ref<i32>, !fir.ref<i32>) {
   !CHECK: ^bb0(%[[VAL_6]]: !fir.ref<i32>, %[[VAL_7]]: !fir.ref<i32>):
   !$omp target
      !CHECK: %[[VAL_8:.*]]:2 = hlfir.declare %[[VAL_6]] {uniq_name = "_QFtarget_unstructuredEi"} : (!fir.ref<i32>) -> (!fir.ref<i32>, !fir.ref<i32>)
      !CHECK: %[[VAL_9:.*]]:2 = hlfir.declare %[[VAL_7]] {uniq_name = "_QFtarget_unstructuredEj"} : (!fir.ref<i32>) -> (!fir.ref<i32>, !fir.ref<i32>)
      !CHECK: ^bb1:
      do while (i <= j)
         !CHECK: ^bb2:
         i = i + 1
      end do
      !CHECK: ^bb3:
      !CHECK: omp.terminator
   !$omp end target
   !CHECK: }
end subroutine target_unstructured
