# Connected client weapon investigation

Discord post1567823820557914143 reports a rare connected client's local gun
staying on Plasma Autorifle while the host sees Sci-Fi Handgun firing. Local
switching/shooting stays blocked until death and respawn. The full attached
clip was reviewed. No resolution comment has been posted.

This branch starts at main831c324a and records investigation only. No gameplay
or network repair is proposed without a failing case.

Fresh Windows baselineSHA256:
`550f988c6ec45e9504d74bcecd9486874928b5272712683b7e9e8b9152ac5936`.
The supported build/native checks passed with the documented local ignored
seven-generated-unit Od/Ob0 workaround for MSVC's existing default-O2 heap
failure. Its multiplayer seal was checked before starting both instances.

The initial paired Story harness never created a lobby: normal release builds
intentionally disable the opt-in Story chooser through pc_feature_flags.h.
That rejected harness route is not evidence of the reported weapon defect.

Two actual Arcade System Link instances then joined through ordinary menu
input on isolated loopback peers127.0.0.2/3. Each used an independent copied
Test Lab profile/settings, run lock, process identity and synthetic input file.
No guest state was written or external lobby contacted.

Host Original60/client native180 observations:
- A settled sixteen-cycle switch/fire sequence had sixteen matching guest
  hand, linked weapon-state and host remote-actor weapon IDs. Fourteen rounds
  of ammunition were consumed;96/160 sampled guest updates had zero whole ticks.
- A second forty-eight-cycle run used rapid team confirmation, overlapping
  switch/fire input and host pause/unpause. All forty-eight settled comparisons
  matched; sixteen rounds were consumed and315/480 guest samples had zero whole
  ticks. Both instances stayed on scene29. Actual weapon IDs exercised were14
  and37; no assumption is made that all inventory/weapon combinations were tested.

The tests retained fresh gameplay pictures, raw before/after ammunition and
weapon states, requested rates, executable identity, phase/scene observations
and bounded cleanup. Both owned instances exited and their test locks cleared.
Read-only fields follow the existing stability_transitions_game.py contracts:
actor-pool entity at offset0, global slot determined from the owned context's
entity, first-person hand+30 and linked-state+30, remote actor right-hand+d4+130.
Asynchronous reads are evaluated after input settles, not during a switch edge.

The rare lock did not reproduce. Same-machine LAN does not certify the Internet
bridge, packet-loss/rejoin behavior or every pickup/death sequence. Existing
Story first-person world-identity/rejoin fixes are scoped to Story and cannot
be assumed to solve this Arcade report. Local ignored evidence is retained in
logs/client-weapon-pair and logs/client-weapon-rapid-team. A useful next failure
capture should retain the owned global slot, hand/linked-state IDs and weapon
model identity alongside the host's same-slot actor state before respawn.
