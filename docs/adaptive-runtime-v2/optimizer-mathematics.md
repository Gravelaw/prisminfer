# Optimizer Mathematics and Calibration

This document is the sole V2 authority for notation, equations, objectives, and
statistical definitions. Numeric project thresholds live in
[`evidence-thresholds-and-security.md`](evidence-thresholds-and-security.md).

All inline mathematics uses dollar delimiters. Every display equation is
separated from prose by blank lines and uses a double-dollar block so it renders
in VS Code Markdown preview.

## Units and provenance

Every numeric field declares a unit and one provenance class: configured,
predicted, observed, or inferred. Missing is a state, never zero.

Core units are:

- bytes for capacity, residency, payload, workspace, and traffic;
- seconds for durations and schedule times;
- bytes per second for bandwidth demand and capacity;
- operations per second for compute demand and capacity;
- joules for energy;
- tokens per second for committed throughput;
- nats per token for natural-log cross-entropy and KL diagnostics;
- bits per scalar for representation rate.

No constraint compares unlike units. No normative objective adds seconds, bytes,
joules, and dimensionless quality terms.

## Exact cell

Let $\chi$ identify the immutable validation cell. Its components are the
hardware and host fingerprint; runtime family, backend, revision, and
native/WSL/Linux execution mode; model and source revision; derived artifact,
recipe, and quantized-tensor semantics; tokenizer/template; context;
prompt/task stratum; requested/effective cap identity; power/thermal policy;
software/provider versions; quality contract; measurement protocol; and the
complete service identity.

Separate runtime identity from the remaining exact identity as:

$$
\chi = (\rho,\zeta),
$$

where $\rho$ is runtime family, backend, and exact revision/build, while
$\zeta$ retains every other exact cell field, including OS execution mode and
the exact derived-artifact identity.

The service identity includes request concurrency, arrival process,
scheduler/batching/chunking policy, prefix/KV-cache configuration and observed
cold/warm/hit/eviction state, streaming/output policy, and output cap. A result
outside $\chi$ is a different result. Missing material identity prevents a direct
same-cell comparison.

For a result $y$ produced in cell $\chi_y$, a within-runtime same-cell control
requires:

$$
\operatorname{same}(y,\chi)
\iff
\chi_y = \chi.
$$

An external runtime necessarily has a different $\rho$ and therefore a
different exact cell. Define $\kappa(\chi)$ as the paired-comparator projection:
it removes runtime family/backend/revision/build and exact derived-artifact
bytes/format while retaining hardware/host identity, OS execution mode,
canonical model/source identity, tokenizer/template, prompt/task and service
profile, cap and power/thermal policy, non-runtime software/provider identity,
quality contract, and measurement protocol. Let
$E_{\mathrm{artifact}}(\chi_a,\chi_b)$ be the separately reviewed decision that
the two derived artifacts and quantized-tensor semantics are equivalent for the
declared comparison. A paired direct runtime comparison requires:

$$
\operatorname{pairedDirect}(\chi_y,\chi)
\iff
\rho_y \ne \rho
\;\land\;
\kappa(\chi_y)=\kappa(\chi)
\;\land\;
E_{\mathrm{artifact}}(\chi_y,\chi)=\operatorname{pass}.
$$

This predicate supports only an explicitly labelled paired-runtime comparator
claim. It never makes $\chi_y$ the same cell, and it cannot mix calibration,
plans, feasible sets, or oracle candidates across runtimes.

Let $\Pi_{\mathrm{ack}}(\chi)$ be the set of plans whose actuators are supported
and whose requested values can be acknowledged by the pinned runtime.

Runtime family, backend, OS execution mode, scheduler family, and artifact
identity are fixed components of $\chi$; they are not members of plan $\pi$ or
$\Pi_{\mathrm{ack}}(\chi)$. In particular, if runtime identity $\rho$ changes,

$$
\rho' \ne \rho
\Longrightarrow
\chi' \ne \chi.
$$

Switching any fixed component creates a new cell, calibration partition,
feasible set, and review decision. The optimizer and feasible oracle never mix
candidates from different runtime cells.

## Representation selection and residency

Let $u \in \mathcal U$ denote an indivisible placement unit, $r \in \mathcal R_u$
a representation of that unit, and $d \in \mathcal D$ a device or host tier.
Use binary variables $q_{u,r}$ for representation selection and $x_{u,r,d}$ for
residency.

$$
\sum_{r \in \mathcal R_u} q_{u,r} = 1
$$

$$
x_{u,r,d} \le q_{u,r}
$$

$$
\sum_{d \in \mathcal D} x_{u,r,d} \ge q_{u,r}
$$

Replication is represented by more than one nonzero $x_{u,r,d}$ and must be
charged on every tier. Sharding changes the definition of $\mathcal U$; it is
not hidden inside a fractional placement value.

For the static controller, $q_{u,r}$ is a fixed artifact-cell input. It is not a
Phase 7 actuator. Plan issue #94 may introduce new derived representations and
therefore new cells.

## Effective representation rate

For $N$ represented scalars, operational effective rate includes every retained
representation byte:

$$
R_{\mathrm{eff}}
=
\frac{
8\left(
B_{\mathrm{payload}}
+B_{\mathrm{metadata}}
+B_{\mathrm{index}}
+B_{\mathrm{padding}}
+B_{\mathrm{residual}}
+B_{\mathrm{codebook}}
+B_{\mathrm{checksum}}
\right)
}{N}
\quad
\text{bits per scalar}.
$$

Source duplication, persistent decoded copies, runtime workspace, and transfer
staging are not hidden in $R_{\mathrm{eff}}$. They are charged separately in the
memory and resource DAG. A format does not earn a capacity claim if the source
or a full decoded tensor remains resident.

For a declared discrete symbol stream $Z$ with mass function $p(z)$, its
base-two entropy is:

$$
H_2(Z)
=
-\sum_z p(z)\log_2 p(z)
\quad
\text{bits per symbol}.
$$

$H_2(Z)$ is a coding diagnostic, not an achieved byte rate or speed prediction.
Rate-distortion theory defines:

$$
R(D)
=
\inf_{
p(\widehat x \mid x):
\mathbb E[d(X,\widehat X)] \le D
}
I(X;\widehat X).
$$

The distortion measure, source distribution, and task-quality evaluation remain
explicit. A theoretical or tensor-level distortion bound does not replace the
paired quality fixtures.

## Rotation identities and legality

For an invertible matrix $R$ and continuous random vector $X$:

$$
h(RX)
=
h(X)+\log\lvert\det R\rvert.
$$

If $R$ is orthogonal, $\lvert\det R\rvert=1$, so joint differential entropy is
unchanged. With row-vector convention, a legal absorbed linear transform is:

$$
x' = xR,
\qquad
W' = R^\mathsf{T}W,
\qquad
x'W' = xW.
$$

For compatible attention coordinates:

$$
Q' = QR,
\qquad
K' = KR,
\qquad
Q'K'^\mathsf{T}=QK^\mathsf{T}.
$$

These identities do not authorize a transform through a nonlinearity:

$$
\phi(xR) \ne \phi(x)R
\quad
\text{in general}.
$$

Every candidate must declare the exact absorption/inverse map for residual
branches, nonlinearities, RoPE, attention, and mutable state.

## Capacity constraints

Let $S_{u,r,d}$ be the charged resident bytes for unit $u$, representation $r$,
on tier $d$. The admitted peak is:

$$
M_d(\pi,\chi)
=
\sum_{u \in \mathcal U}
\sum_{r \in \mathcal R_u}
S_{u,r,d}x_{u,r,d}
+M_{\mathrm{state},d}
+M_{\mathrm{workspace},d}
+M_{\mathrm{runtime},d}
+M_{\mathrm{queue},d}
+M_{\mathrm{batch},d}
+M_{\mathrm{cache},d}
+M_{\mathrm{fragmentation},d}
+M_{\mathrm{instrumentation},d}
+M_{\mathrm{unknown},d}
+M_{\mathrm{safety},d}.
$$

Promotion requires:

$$
M_d(\pi,\chi) \le C_{\mathrm{effective},d}(\chi)
\qquad
\text{for every participating tier } d.
$$

Promoted unknown bytes are zero. GPU, pageable host, pinned host, process
commit, and storage constraints are separate; pagefile/commit does not increase
physical resident capacity.

$M_{\mathrm{state},d}$ covers active per-request architecture state;
$M_{\mathrm{queue},d}$ covers scheduler and request-queue state;
$M_{\mathrm{batch},d}$ covers batching/chunking pools and their workspaces; and
$M_{\mathrm{cache},d}$ covers retained shared prefix/KV allocations, indices,
and eviction workspace; and $M_{\mathrm{safety},d}$ covers the declared
telemetry safety margin. These terms, $M_{\mathrm{runtime},d}$, the resident
representation sum, fragmentation, instrumentation, and unknown bytes are
charged once under mutually exclusive ledger categories. A disabled feature has
an observed or configured zero only when the exact runtime path acknowledges
that state; a missing field remains unknown and blocks promotion.

## Resource DAG and overlap

Let each action $a$ have start time $s_a$, measured or conservatively predicted
duration $t_a$, and resource-rate demand $\beta_{a,\ell}(t)$ on resource
$\ell$. For every dependency edge $a \rightarrow b$:

$$
s_b \ge s_a+t_a.
$$

If $C_\ell(t)$ is the capacity of resource $\ell$ in the same rate unit:

$$
\sum_{a\ \mathrm{active\ at}\ t}
\beta_{a,\ell}(t)
\le
C_\ell(t).
$$

The makespan is:

$$
T_{\mathrm{DAG}}
=
\max_a(s_a+t_a)-\min_a s_a.
$$

PCIe, CPU decode, GPU compute, storage fetch, and copy-engine overlap are
credited only by this schedule and retained timeline evidence. Summing isolated
stage durations is a conservative bound, not proof of exposed latency.

## Feasibility and objective

Let each hard invariant be $g_j(\pi,\chi)\le0$. The feasible set is:

$$
\Pi_{\mathrm{feas}}(\chi)
=
\left\{
\pi \in \Pi_{\mathrm{ack}}(\chi):
g_j(\pi,\chi)\le0
\ \text{for every } j
\right\}.
$$

If $\Pi_{\mathrm{feas}}(\chi)$ is empty, the controller abstains, selects the
declared upstream baseline if it is itself feasible, or rejects the cell. It
never relaxes a safety or quality invariant.

Within the feasible set, retain the Pareto frontier. Select with a predeclared
lexicographic order over quantities such as p95 request latency, negative
committed throughput, energy, and external bytes:

$$
J(\pi,\chi)
=
\left(
L_{\mathrm{request,p95}},
-\Theta_{\mathrm{committed}},
E_{\mathrm{request}},
B_{\mathrm{external}}
\right).
$$

The priority order is frozen before the evaluation. Any scalar tie-breaker must
first normalize every term against a frozen same-unit reference and is
diagnostic, not the normative objective.

## Calibration and regret

Calibration partitions are nested by session or other independent unit. Search,
validation, confirmation, and promotion data are distinct. Let
$\widehat T_k(\pi,\chi)$ predict stage $k$ and $T_k(\pi,\chi)$ be the observed
value.
Relative absolute prediction error is:

$$
e_k(\pi,\chi)
=
\frac{
\lvert \widehat T_k(\pi,\chi)-T_k(\pi,\chi)\rvert
}{
\max(T_k(\pi,\chi),\varepsilon_T)
}.
$$

The feasible measured oracle for a held-out cell is:

$$
\pi^\star(\chi)
\in
\operatorname*{arg\,min}_{\pi\in\Pi_{\mathrm{feas}}(\chi)}
J(\pi,\chi).
$$

For committed throughput $\Theta$, selector regret is:

$$
\operatorname{Regret}_{\Theta}(\chi)
=
\frac{
\Theta(\pi^\star,\chi)-\Theta(\widehat\pi,\chi)
}{
\max(\Theta(\pi^\star,\chi),\varepsilon_\Theta)
}.
$$

Finalists are rerun fresh. The oracle is feasible only over the measured,
acknowledged candidate set within one immutable runtime cell; it is not a global
optimum or cross-runtime selection claim.

## Empirical quality promotion

For independent held-out units $i=1,\ldots,n$, define a violation against the
frozen reference:

$$
V_i
=
\mathbf 1
\left[
d(Y_{\pi,i},Y_{0,i})>\epsilon
\right],
\qquad
k=\sum_{i=1}^{n}V_i.
$$

Let $U_{1-\alpha}(k,n)$ be the one-sided exact binomial upper confidence bound.
Promotion requires:

$$
U_{1-\alpha}(k,n)\le\delta.
$$

The values of $\epsilon$, $\delta$, $\alpha$, sample size, strata, exclusions,
and stop rule are frozen in the sample plan. This is a bounded empirical claim,
not proof over all prompts. Task-score regression and worst-stratum results are
reported separately.

With natural logarithms, token cross-entropy is:

$$
\operatorname{CE}_{\pi}
=
-\frac{1}{N}
\sum_{i=1}^{N}
\log p_{\pi}(y_i\mid y_{<i},c_i)
\quad
\text{nats per token}.
$$

Forward KL from the frozen reference distribution is:

$$
D_{\mathrm{KL}}(p_0\Vert p_\pi)
=
\sum_v
p_0(v)
\log\frac{p_0(v)}{p_\pi(v)}.
$$

Cross-entropy and KL are diagnostics. They do not replace generation, retrieval,
task, and output-length evaluation.

## Progressive representation

The general prefix decoder is:

$$
\widehat W^{(m)}
=
\operatorname{Decode}(B_0,R_1,\ldots,R_m),
\qquad
m\in\{0,\ldots,M\}.
$$

For a specifically additive codec, a canonical offline construction may use:

$$
\widehat W^{(0)}=D_0(C_0).
$$

$$
E_j=W-\widehat W^{(j-1)},
\qquad
C_j=Q_j(E_j).
$$

$$
\widehat W^{(j)}
=
\widehat W^{(j-1)}+D_j(C_j).
$$

Monotone tensor distortion and monotone task quality are hypotheses to test, not
assumptions. Runtime compute must consume the base/prefix directly or by bounded
tile scratch; full-model reconstruction invalidates the hot-path claim.

## Compression and transfer break-even

For immutable weights encoded offline, online encode time is zero. A serialized
screening inequality is:

$$
\frac{B_c}{W}
+T_{\mathrm{decode}}
+T_{\mathrm{transform}}
<
\frac{B_0}{W}.
$$

Here $B_0$ and $B_c$ are bytes, $W$ is measured bytes per second, and times are
seconds. The resource DAG is authoritative when stages overlap.

For online activation-transfer compression:

$$
T_{\mathrm{encode}}
+\frac{B_c}{W}
+T_{\mathrm{decode}}
+\Delta T_{\mathrm{consumer}}
<
\frac{B_0}{W}.
$$

The threshold registry requires a p95 margin; a mean-only win does not pass.

## KV and architecture-state accounting

For batch $N$, context $T$, layer-specific KV head count $H_{\mathrm{kv},\ell}$,
head dimension $d_\ell$, and key/value rates $r_{K,\ell}$ and $r_{V,\ell}$ in
bits per scalar, payload bytes are:

$$
B_{\mathrm{KV,payload}}(T)
=
\frac{NT}{8}
\sum_{\ell}
H_{\mathrm{kv},\ell}d_\ell
\left(
r_{K,\ell}+r_{V,\ell}
\right).
$$

Scales, zero points, residual windows, indexes, allocator granularity,
fragmentation, and workspaces are added separately. Ornith also requires
separate DeltaNet/recurrent, convolution, MTP, and optional multimodal state
terms.

For one attention score, a useful diagnostic bound is:

$$
\left|
q^\mathsf{T}k-q^\mathsf{T}\widehat k
\right|
\le
\lVert q\rVert_2
\lVert k-\widehat k\rVert_2.
$$

It does not bound softmax or end-to-end generation quality by itself.

## Committed-output-aware speculation

For independent cycles or requests $i$, let $G_i$ be committed
target-distributed output tokens, $T_i$ elapsed seconds, and $B_i$ observed
external bytes. Primary pooled estimators are:

$$
\widehat\Theta_{\mathrm{committed}}
=
\frac{\sum_i G_i}{\sum_i T_i}.
$$

$$
\widehat B_{\mathrm{per\ committed\ token}}
=
\frac{\sum_i B_i}{\sum_i G_i}.
$$

Also report request-level distributions, accepted draft length, verification
cost, rollback/recompute, and additional draft-model memory. Accepted drafts
alone are never the numerator of a performance claim.

## Structured-compute oracle and router

Transformer blocks are sequential. A gated residual recurrence is:

$$
h_{b+1}
=
h_b+z_b(h_b)F_b(h_b),
\qquad
z_b(h_b)\in\{0,1\}.
$$

First compute an offline oracle over hardware-aligned units under the frozen
quality rule. Only if the oracle passes the removal threshold may a router be
trained. Router realization is:

$$
\rho_{\mathrm{realized}}
=
\frac{
\text{work saved by the guarded router}
}{
\text{work saved by the oracle}
}.
$$

Router inference, gather/scatter, dense audits, pre-commit verification,
out-of-distribution behavior, and any post-commit approximation are charged.
Post-commit auditing can stop future use but cannot undo released tokens or
mutated state.
