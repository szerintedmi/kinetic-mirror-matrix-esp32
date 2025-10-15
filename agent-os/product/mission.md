# Product Mission

## Pitch
Mirror Array is a hackable kinetic mirror kit that helps makers and creative coders craft repeatable reflection patterns by providing precise, scalable motion control with instant serial control today and effortless wireless orchestration later.

## Users

### Primary Customers
- Makers and hardware hackers: Build reliable, motor-driven mirror experiments and weekend demos without fighting calibration.
- Creative coding crews: Prototype interactive light walls that respond to code, sensors, or live inputs and scale cleanly.

### User Personas
**Garage Light Hacker** (25-38)
- **Role:** Nights-and-weekends tinkerer who loves steppers, lasers, and flashy demos.
- **Context:** Works in a cramped workshop with DIY rigs and a rotating cast of friends dropping in.
- **Pain Points:** Manual mirror tweaks, hard-to-repeat light moves, and no easy way to share setups.
- **Goals:** Spin up reliable mirror choreographies fast enough to show off at the next meetup.

**Creative Coding Duo** (28-45)
- **Role:** Small team that blends projection, sensors, and code-driven visuals.
- **Context:** Tours pop-up experiences, juggling budget gear and tight setup windows.
- **Pain Points:** Fragile mirror rigs, time-consuming calibration, and few chances to test before going live.
- **Goals:** Load repeatable reflection patterns and riff on them with code or audience input.

**Weekend Pattern Tinkerer** (20-40)
- **Role:** Solo experimenter who spends rainy afternoons scripting motion sequences.
- **Context:** Hops between open-source tools and quick breadboard hacks in a tiny apartment lab.
- **Pain Points:** Hard-to-adjust mounts, minimal motion ranges, and no easy way to preview custom sequences.
- **Goals:** Snap together a rig, try wild motion scripts, and share the best ones without building new hardware.

**Installation Superfan** (All ages)
- **Role:** Visitor or passerby who loves watching kinetic light shows.
- **Context:** Encounters pop-up installations in galleries, plazas, or meetups.
- **Pain Points:** Static displays that burn out quickly and don’t react to the crowd.
- **Goals:** Get mesmerized by evolving reflections and see new patterns every time they drop in.

## The Problem

### DIY Mirror Chaos
Most hacker-grade mirror projects need hours of fiddly alignment and still drift off target the moment someone nudges the rig—costing 30–60 minutes per setup and frequent do-overs.

**Our Solution:** A modular mirror deck driven by a single ESP32 controlling an 8‑mirror cluster out of the box. Clear serial commands (MOVE, HOME, STATUS, WAKE/SLEEP) make it easy to script reliable moves from any laptop today, with a smooth path to wireless broadcast control when you scale.

## Differentiators

### Hackable Paths To Scale
There’s no direct rival that mixes modular mirror decks, fine‑grained motion, and open scripting. Unlike one‑off art builds or binary mirror walls, we keep the control flow consistent from a single 8‑mirror cluster to larger arrays. Start with simple serial scripts; later broadcast the same commands wirelessly to multiple clusters. Result: faster setup days and more time tweaking the look, not the hardware.

## Key Features

### Core Features
- **8‑mirror starter cluster:** Unbox a single controller that runs eight mirrors cleanly so you can create patterns on day one.
- **Repeatable mirror cues:** Save and replay reflection patterns that land in the same spot every time, even after teardown.
- **One-button homing:** Kick off homing and know every mirror starts from a solid baseline.
- **Simple serial control:** Drive MOVE, HOME, STATUS, and WAKE/SLEEP from any laptop or script—no custom tooling required.
- **Quiet, cool operation:** Per‑mirror sleep and smart motion keep noise and heat down so shows run longer.

### Collaboration Features
- **Preset sharing packs:** Swap scenes with friends so they can remix your favorite light sweeps in their own setups.
- **Quick health checks:** Glance at STATUS to spot lazy mirrors before inviting a crowd over.
- **Script-first workflow:** Share plain‑text command scripts that run the same on every cluster.

### Advanced Features
- **Broadcast multi‑cluster sync:** Upgrade to wireless orchestration (ESP‑Now) that reuses the serial command protocol for bigger walls.
- **Scale beyond 8:** Explore driving 16+ mirrors per controller and coordinated step generation for identical motions.
- **Live play modes:** Let guests “paint” with their reflection using joysticks, sensors, or code hooks.
- **Scene sandboxing:** Test reachability and timing in software before committing to a build session.
