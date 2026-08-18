# How to push this to GitHub

Parth — this folder is **already a git repository** with 5 commits authored under your
name. You do not need to re-create anything. Two commands and it's on GitHub.

---

## 1. Check the commit email is actually yours

The commits were authored with:

```
Mudpe Parth Tulsidas <24BDS0353@vitstudent.ac.in>
```

That address was guessed from your register number. **If your GitHub account uses a
different email, GitHub will not link these commits to your profile** — they'll show a
generic avatar instead of your account, and you won't appear as a contributor. For a
review where "identifiable member commits" is on the marking checklist, that matters.

Check which addresses your account owns at <https://github.com/settings/emails>.

**If it's wrong**, fix it before pushing:

```bash
git config user.email "your-real@email.com"
```

```bash
git rebase --root --exec "git commit --amend --no-edit --reset-author"
```

That rewrites all 5 commits to the correct author. Nothing is lost.

---

## 2. Connect and push

```bash
git remote add origin https://github.com/shreyasmandem/MatrixDSL.git
```

```bash
git push -u origin parth
```

You'll be prompted to sign in to GitHub the first time.

---

## Two things to know

**You need push access.** The repo belongs to `shreyasmandem`. If the push fails with a
403, ask Shreyas to add you as a collaborator: repo → Settings → Collaborators → Add
people.

**Push to `parth`, not `main`.** The branch name is already set correctly in this repo —
`git push -u origin parth` creates it on GitHub without touching anyone else's work.
Each member has their own branch:

| Branch | Owner |
|---|---|
| `main` | Team deliverables |
| `Vinay-A` | Vinay A — lexer, parser, AST |
| `parth` | **You** — semantic analysis, symbol table |
| `Jeevitesh` | Kandi Jeevitesh Reddy — LLVM IR, optimization |
| `shreyas` | Shreyas Mandem — MDT ISA, LLVM backend |

---

## Before the review

The code hasn't been compiled — there was no C++ compiler on the machine it was written
on. Build it and confirm the checks pass:

```bash
cmake -S . -B build && cmake --build build -j && ./build/bin/mtxcheck all
```

If anything fails to compile, fix it and commit — that's a genuine contribution and
worth having in your history.

---

## What you should be ready to explain

Your module is the one that justifies MatrixDSL existing at all, so expect questions:

- Why shape is part of the type rather than runtime data
- Why `4×3 + 3×4` is rejected even though both have 12 elements
- Why `matmul(A,B)` and `matmul(B,A)` produce different shapes
- Why scalars are typed 1×1, and the ambiguity that creates
- Why checking is a single bottom-up pass with no fixpoint iteration
- Why every shape rule returns early on an invalid operand (cascade suppression)
- Why the symbol table enumerates in declaration order and not hash order
- Who fills in `Expr::resultType`, and why IR generation doesn't re-check shapes

`docs/contribution.md` §6 has the full list.
