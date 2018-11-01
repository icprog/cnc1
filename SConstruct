################################################################################
# Filename:    SConstruct                                                      #
# License:     Public Domain                                                   #
# Author:      New Rupture Systems                                             #
# Description: Build CNC 1 project binaries and utilities (Device and Host).   #
################################################################################
import os
import sys
import platform

# Verify SCons version is high enough to run this build script
EnsureSConsVersion(3, 0, 1)

# Project name
project = "CNC 1"

# Modules that can be built (relative path to SConscript file) and known aliases
# for each module
modules = ((Dir("Host"), "cncControl"),
           (Dir("Device").Dir("ATtiny25"), ()),
           (Dir("Device").Dir("ATmega324").Dir("Bootloader"), "Bootloader"),
           (Dir("Device").Dir("ATmega324").Dir("Application"), "Application"))

# Supported target platforms
targets = {"Linux"   : ("x86_64", "GNU/Linux", 4.2),
           "Windows" : ("x86_64", "Windows", 6.1)}

# Default target platform (if the native one cannot be determined)
default_target = "Linux"

# Custom tools location(s)
custom_tools_path = (Dir("#").Dir("Tools").Dir("SCons"),)

# Global variables
output_header = False
spin = 0
command = ""
error = ""


#
# Determine if one or more specified targets(aliases) are specified for the
# specified module.
#
# INPUT : module - module(script, aliases) that may specifiy target
#         targets - Targets to determine if present in module
# OUTPUT: [Return] - True if target present in module, False otherwise
#
def target_in_module(module, targets):
   in_module = False
   script = module[0]
   try:
      _ = iter(module[1])
      if type(module[1]) is str:
         raise TypeError()
      aliases = module[1]
   except:
      aliases = (module[1],)

   for target in targets:
      if ((str(script).startswith(str(target))) or
          (str(target) in aliases)):
         in_module = True
         break

   return in_module


#
# Output build target info.
#
# INPUT : env - SCons environment
#         targets - Targets to index modules and list related targets
# OUTPUT: [None]
#
def list_targets(env, targets):
   print("\nTargets:")

   mods = []
   for mod in modules:
      if target_in_module(mod, targets):
         mods.append(mod)

   if not mods:
      mods = modules

   for mod in mods:
      print((" " * 3) + "Module: " + str(mod[0]))
      try:
         _ = iter(mod[1])
         if type(mod[1]) is str:
            raise TypeError()
         aliases = mod[1]
      except:
         aliases = (mod[1],)
      for alias in aliases:
         print((" " * 6) + alias)


#
# Output dependency info.
#
# INPUT : env - SCons environment
#         depends - Dependencies to list
# OUTPUT: [None]
#
def list_dependencies(env, depends):
   print("\nDependencies:")

   strict_depends = {}
   for listing in depends:
      for t, names in listing.items():
         if t not in strict_depends:
            strict_depends[t] = set(names)
         else:
            strict_depends[t].update(names)

   for t, names in strict_depends.items():
      print((" " * 3) + t.title() + ":")
      for name in names:
         print((" " * 6) + name)


#
# Callback executed when a command (buildling a target, performing an action,
# etc.) string should be output.
#
# INPUT : s - Command string to output
#         target - Target command string relates to
#         source - Source command string relates to
#         env - SCons environment
# OUTPUT: [None]
#
def output_cmd(s, target, source, env):
   global output_header
   global spin
   global command
   spinner = ['-', '\\', '|', '/']

   if env["VERBOSITY"] != 2:
      if (not target or
          s.startswith("Mkdir")):
         return

   # Output header as first output
   if output_header:
      output_header = False
      print(("=" * 50))
      print("Project: {0}".format(env["PROJECT_NAME"]))
      print("Build:   " + ("Debug\n" if env["DEBUG"] else "Release"))
      print("Target:  {0}".format(env["TARGET_NAME"]))
      print(("=" * 50))

   if env["VERBOSITY"] == 0:
      # Max line is 80 characters
      if len(s) > 76:
         s = (s[:-(len(s) - 73)]) + "..."

      s = "[" + spinner[spin] + "] " + s
      spin = (spin + 1) % 4
      sys.stdout.write("\r" + s)
      if len(s) < len(command):
         sys.stdout.write(" " * (len(command) - len(s)))
         sys.stdout.write("\b" * (len(command) - len(s)))
   else:
      sys.stdout.write(s + "\n")
   command = s
   sys.stdout.flush()


#
# Callback executed when the last target has finished building.
#
# INPUT : target - N/A
#         source - N/A
#         env - SCons environment
# OUTPUT: [None]
#
def build_finished(target, source, env):
   global command

   # Output a newline if using verbosity=0
   if env["VERBOSITY"] == 0 and command:
      sys.stdout.write("\n")


try:
   # Specify build options
   Help("\nBuild options:\n"
        "   --build=<scons-platform>  Specify build platform\n"
        "   --target=<platform>       Specify target platform\n"
        "   --prefix=<directory>      Specify build directory\n"
        "   --tools=<tools>           Specify comma-separated build tools\n"
        "   --debug-info              Build with debug information\n"
        "   --verbosity=<0-2>         Specify build log verbosity\n"
        "   --list-dependencies       Output build target dependencies\n"
        "   --list-targets            Output build targets\n"
        "   --help                    Output this help message\n"
        "\nSupported target platforms:\n"
        "   Linux   - GNU/Linux OS\n"
        "   Windows - Microsoft Windows OS (experimental)\n")
   AddOption("--build",  dest = "build")
   AddOption("--target", dest = "target")
   AddOption("--prefix", dest = "build-dir", default = Dir("#"))
   AddOption("--tools", dest = "tools")
   AddOption("--debug-info", dest = "debug", action = "store_true")
   AddOption("--verbosity", dest = "verbosity", type = "int")
   AddOption("--list-dependencies", dest = "list-depends",
             action = "store_true", default = False)
   AddOption("--list-targets", dest = "list-targets", action = "store_true",
             default = False)

   # Exit if "--help" argument present
   if GetOption("help"):
      Exit(0)

   # Build custom tools list
   custom_tools = set()
   for base_dir in custom_tools_path:
      base_dir = str(base_dir)
      dirs = [tuple()]
      while len(dirs):
         dir_list = dirs.pop()
         path = str(base_dir)
         tool_prefix = ""

         # Handle sub-directory of base directory
         for d in dir_list:
            path = os.path.join(path, d)
            tool_prefix += (d + ".")

         # Get all tools (and directories) in directory
         entries = os.listdir(path)
         for entry in entries:
            tool_path = os.path.join(path, entry)
            if os.path.isfile(tool_path):
               if entry.endswith(".py"):
                  custom_tools.add(tool_prefix + entry[:-3])
            elif os.path.isdir(tool_path):
               dirs.append(dir_list + (entry,))
   custom_tools = tuple(custom_tools)

   # Set fallback target to current platform (if possible)
   platform = platform.system()
   if platform in targets:
      default_target = platform

   # Set environment variables from command-line options
   overrides = {}
   if GetOption("target") is not None:
      target = GetOption("target")

      # Validate target
      valid = False
      for t in targets:
         if t.upper() == target.upper():
            overrides["TARGET_NAME"] = target
            overrides["TARGET_ARCH"] = targets[target][0]
            overrides["TARGET_OS"] = targets[target][1]
            overrides["TARGET_OS_VERSION"] = targets[target][2]
            valid = True
            break
      if not valid:
         error = "Unknown target platform"
         Exit(1)
   if GetOption("debug") is not None:
      overrides["DEBUG"] = (1 if (GetOption("debug") == True) else False)
   if GetOption("tools") is not None:
      tools = [s.strip() for s in GetOption("tools").split(",")]
      tools.append("default")
      overrides["TOOLBOX"] = ",".join(map(str, tools))
   if GetOption("verbosity") is not None:
      overrides["VERBOSITY"] = GetOption("verbosity")
      if overrides["VERBOSITY"] < 0 or overrides["VERBOSITY"] > 2:
         error = "--verbosity argument range (0 - 2)"
         Exit(1)

   # Create a suitable environment
   if COMMAND_LINE_TARGETS:
      overrides["CMD_LINE_TARGETS"] = ",".join(map(str, COMMAND_LINE_TARGETS))
      args = [None]
   else:
      args = [Dir(GetOption("build-dir")).File(".sconarg.cache").path]
   args.append(overrides)

   vcache = Variables(*args)
   vcache.AddVariables(("CMD_LINE_TARGETS", "", ""),
                       ("TARGET_NAME", "", default_target),
                       ("TARGET_ARCH", "", targets[default_target][0]),
                       ("TARGET_OS", "", targets[default_target][1]),
                       ("TARGET_OS_VERSION", "", targets[default_target][2]),
                       BoolVariable("DEBUG", "", 0),
                       ("TOOLBOX", "", "default"),
                       ("VERBOSITY", "", 1))

   args = {"variables" : vcache}
   if custom_tools:
      args["tools"] = custom_tools
      args["toolpath"] = custom_tools_path
   if GetOption("build"):
      args["platform"] = GetOption("build")

   env = Environment(**args)

   # Set SCons database location
   env.SConsignFile(Dir(GetOption("build-dir")).File(".sconsign").path)

   # Set environment defaults
   env.Replace(PROJECT_NAME = project,
               LIST_DEPENDS = GetOption("list-depends"),
               PRINT_CMD_LINE_FUNC = output_cmd,
               CONFIGUREDIR = Dir(GetOption("build-dir")).Dir(".sconf_temp"),
               CONFIGURELOG = (Dir(GetOption("build-dir")).Dir(".sconf_temp")
                               .File("configure.log")),
               CPPDEFINES = [("PLATFORM_ARCH", "PLATFORM_{0}".
                              format(env["TARGET_ARCH"]).upper()),
                             ("PLATFORM_OS", "PLATFORM_{0}".
                              format(env["TARGET_OS"]).
                               translate(None, "/\\").upper()),
                             ("PLATFORM_OS_VERSION", env["TARGET_OS_VERSION"])])

   # Set file type command strings
   if env["VERBOSITY"] < 2:
      env.Append(CCCOMSTR = "Compiling $TARGET",
                 ASPPCOMSTR = "Assembling $TARGET",
                 LINKCOMSTR = "Linking $TARGET")

   # Set command-line targets (if previously specified)
   if (("CMD_LINE_TARGETS" in env) and
       (env["CMD_LINE_TARGETS"] != "")):
      COMMAND_LINE_TARGETS = list(env["CMD_LINE_TARGETS"].split(","))

   # Exit if "--list-targets" argument present
   if GetOption("list-targets"):
      list_targets(env, COMMAND_LINE_TARGETS)
      Exit(255)

   # Set default targets to build and clean
   env.Default(Dir("#"))

   # Cache tools (for later configurations)
   env.CacheTools(tuple(env["TOOLBOX"].split(",")))

   # Set SConscript's to read (if specific targets are specified)
   scripts = []
   for mod in modules:
      if target_in_module(mod, COMMAND_LINE_TARGETS):
         scripts.append(mod[0])

   # Read all SConscript's if either no targets are specified or an "unknown"
   # target was specified
   if not scripts:
      for mod in modules:
         scripts.append(mod[0])

   # Run through scripts
   base_env = env
   args = {"exports" : "env"}
   depends = []
   for script in scripts:
      if GetOption("build-dir") != Dir("#"):
         args["variant_dir"] = Dir(GetOption("build-dir")).Dir(str(script))
         args["duplicate"] = 0

      env = base_env.Clone()
      status = base_env.SConscript(Dir(str(script)).File("SConscript"), **args)

      try:
         _ = iter(status)
         if type(status) is str:
            raise TypeError()
         error = status[0]
         output = (status[1],)
      except:
         error = status
         output = ()

      if error:
         Exit(1)
      elif ((GetOption("list-depends")) and
            (len(output) > 0)):
         depends.append(output[0])
   env = base_env

   # Exit if "--list-dependencies" argument present
   if GetOption("list-depends"):
      list_dependencies(env, depends)
      Exit(255)

   # Setup hook to execute when build finishes
   last_target = env.Command("_finish", [],
                             env.Action(build_finished, cmdstr = None))
   env.Depends(last_target, BUILD_TARGETS)
   env.Ignore(last_target, BUILD_TARGETS)
   BUILD_TARGETS.extend(last_target)

   vcache.Save(Dir(GetOption("build-dir")).File(".sconarg.cache").path, env)
   output_header = True
except SystemExit as e:
   if e.code == 255:
      raise
   if e.code and not GetOption("silent"):
      print("Error: {0}".format(error))
      raise
except:
   raise