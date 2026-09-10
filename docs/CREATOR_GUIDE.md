# CourseMaker — creator guide

A short tour of building a shooting course. No coding required.

## Open the editor

* `CourseMaker.exe` — opens the bundled **Marksman Fundamentals** example.
* `CourseMaker.exe C:\path\to\my-course` — opens a project folder.
* **File ▸ New Project…** — pick a name and a parent folder; CourseMaker creates
  `<parent>/<name>/` with `project.json`, a starter environment and one drill.

The window has: **Toolbar** (play controls, gizmo mode, grid snap), **Viewport**
(the 3D view), **Hierarchy** (scene tree), **Inspector** (selected object),
**Assets** (files in the project), **Course** (course + drill settings),
**Diagnostics** (log and validation). Drag panel edges to resize; the layout is
remembered. **View ▸ Reset Layout** restores the default.

## Move around the viewport

Hold the **right mouse button** and steer with **W A S D** (Q / E = down / up).
**Shift** = faster, **Ctrl** = slower. **Middle-drag** = pan, **wheel** = dolly.
Select an object and press **F** to frame it.

## Build a course

1. **Course panel ▸ Drills** — press **+ Add Drill**. Right-click a drill to
   move it up/down, duplicate or delete. There is no fixed limit; validation
   warns past ~64.
2. Click a drill to make it the **active** drill — its targets appear in the
   viewport under `DrillRoot` and are editable like anything else.
3. **Add menu** — drop in targets (circle / square / triangle / knock cube /
   moving / reach zone), shapes, or environment blocks. New objects appear in
   front of the camera and are selected.
4. Select an object and edit it in the **Inspector**: name, transform
   (drag the fields or use the on-screen **gizmo** — W / E / R switch
   move / rotate / scale, Grid Snap in the toolbar), and per-component settings
   (target kind & scoring, mesh size & colour, moving-target path & speed, …).
5. **First-person placement**: double-click a model or image in **Assets** (or
   drag it into the viewport). A ghost follows the crosshair — **wheel** changes
   distance, **R** rotates, **Shift** aligns to the surface, **left click**
   confirms, **Esc** cancels. Every placement is one undo step.
6. **Course panel ▸ Objectives** — add shoot / knock / reach / finish goals, set
   the amount and the HUD text. Diagnostics flags a drill that asks for more
   target hits than it has targets.

## Movement, weapons, audio

* **Course ▸ Movement defaults** — pick a **Feel** preset (Responsive is the
  Counter-Strike-style reference) or set Move Speed, Jump Height (metres),
  accel / decel / friction / air control / sensitivity directly. A drill can
  untick *Inherit project movement* to override.
* **Course ▸ Weapons** — four categories (Pistol, SMG, Automatic Rifle, Sniper).
  Edit fire mode, fire rate, damage, magazine, reload, spread, recoil, and (for
  snipers) aim zoom. Weapons with no imported model use a simple built-in shape.
* **Course ▸ Key Bindings** — rebind weapon slots / next / prev / reload / aim.
  A red field means two actions share a key.
* **Course ▸ Audio mix** and each drill's **Music** field.

## Playtest

Press **Play** in the toolbar to run the active drill. **Pause / Resume**,
**Restart**, **Stop**. Play runs on a throw-away copy of the scene, so **Stop**
restores exactly what you authored — target positions, deleted objects and all.
Press **Esc** while playing to stop.

## Save, share, move

**File ▸ Save** (Ctrl+S) writes `project.json`, every drill file and the
environment scene, and drops a timestamped copy in `backups/`. The whole project
folder is portable: zip it, move it, hand it to someone else, and
**File ▸ Open Project…** reopens it — every reference inside is folder-relative.

## When something is wrong

The **Diagnostics** panel collects engine messages and validation results.
Press **Revalidate** any time. Malformed files, missing assets and impossible
drill setups are reported there rather than crashing the editor.
