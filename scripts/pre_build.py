import os
Import("env")

# Remove the HFSM library's placeholder src/main.cpp that conflicts
# with the project's own setup()/loop() definitions.
libdeps_dir = env.subst("$PROJECT_LIBDEPS_DIR")
env_name = env.subst("$PIOENV")
hfsm_main = os.path.join(libdeps_dir, env_name, "HFSM", "src", "main.cpp")

if os.path.isfile(hfsm_main):
    os.remove(hfsm_main)
    print(f"pre_build: Removed conflicting {hfsm_main}")

