################################################################################
# Filename:    ConfigureEx.py                                                  #
# License:     Public Domain                                                   #
# Author:      New Rupture Systems                                             #
# Description: Defines functions and methods for using the Extended            #
#              Configuration (ConfigureEx) tool.                               #
################################################################################
import os
import os.path
from collections import OrderedDict
from SCons.Script import *
from SCons.SConf import SConfBase
from SCons.SConf import progress_display
from SCons.SConf import SetProgressDisplay

# Display function (may be disabled in class "ConfExBase")
def _ConfExDisplay(msg):
   print msg

ConfExDisplay = _ConfExDisplay


#
# Validate linker. Linker is considered valid if an attempt to link a test file
# is successful. In addition, the output file created by the linker may be
# validated if the parameters 'output_format' and 'output_isa' are set (either
# explicitly or via the default arguments). The special (default) value
# '<target>' mat be specified for either parameter which will resolve to the
# current target format (via the default format for the target OS) and target
# ISA respectively.
#
# INPUT : context - SCons Configure() test context
#         src_build - Input to test linker. May be a known string (in which
#                     case a pre-defined link using a known input component will
#                     be used) or a callable (which must return a list of
#                     objects that will attempt to be linked together)
#         output_format - Expected linker output file format (or '<target>')
#         output_isa - Expected linker output file ISA (or '<target>')
# OUTPUT: [Return] - int(True) if linker valid, int(False) otherwise
#
def CheckLink(context, output_format = "<target>", output_isa = "<target>",
              src_build = "C"):
   src_ext = {"C" : ("\nint main()\n{\n    return 0;\n}\n", ".c")}
   file_format = ("PE", "ELF")
   file_isa = ("x86", "x86_64", "AVR")
   isa_suffix = {file_isa[0] : "32",
                 file_isa[1] : "64"}
   os_formats = {"Windows"   : file_format[0],
                 "GNU/Linux" : file_format[1],
                 "BSD"       : file_format[1]}
   elf_isa = {0  : "None",
              3  : file_isa[0],
              62 : file_isa[1],
              83 : file_isa[2]}

   pe_isa = {0    : "None",
            332   : file_isa[0],
            34404 : file_isa[1]}
   link_file = None
   check_passed = False

   # Resolve parameters
   if output_format == "<target>":
      if context.env["TARGET_OS"] in os_formats.keys():
         output_format = os_formats[context.env["TARGET_OS"]]
      else:
         raise ValueError("Target OS linking output format unknown")
   if output_isa == "<target>":
      output_isa = context.env["TARGET_ARCH"]

   # Check parameters
   if ((not callable(src_build)) and
       (src_build not in src_ext.keys())):
         raise ValueError("Specified source build unknown")
   if (output_format and
       (output_format not in file_format)):
      raise ValueError("File format unknown")
   if (output_isa and
       (output_isa not in file_isa)):
      raise ValueError("File ISA unknown")

   context.Message("Checking whether the linker ({0}-{1}{2}) works... "
      .format(output_isa, output_format, isa_suffix[output_isa]
         if output_isa in isa_suffix else ""))
   try:
      if callable(src_build):
         # Try to build up an action string to link all objects together. Given
         # the large variety of ways that LINKCOM can be specified the below is
         # a hopeful attempt at covering the most common "plain string" case
         action = str(context.env["LINKCOM"])
         sources = ""
         objs = src_build(context)

         for obj in objs:
            sources += (" " + str(obj))

         action = action.replace("$SOURCES", sources)
         builder = Builder(action = action)
         context.env.Append(BUILDERS = {'CustomProgram' : builder})
         linked = context.TryBuild(context.env.CustomProgram)
      else:
         linked = context.TryLink(src_ext[src_build][0], src_ext[src_build][1])

      if linked:
         if not output_format and not output_isa:
            check_passed = True
         else:
            link_file = open(str(context.lastTarget), "rb")
            magic_header = link_file.read(4)
            if magic_header == "\x7FELF":
               if not output_format or output_format == file_format[1]:
                  if not output_isa:
                     check_passed = True
                  else:
                     link_file.seek(5)
                     endian = int(link_file.read(1).encode("hex"), 16)
                     link_file.seek(20)
                     version = int(link_file.read(4).encode("hex"), 16)
                     if endian == 1:
                        version = (version >> 24)
                     if version == 1:
                        link_file.seek(18)
                        isa = int(link_file.read(2).encode("hex"), 16)
                        if endian == 1:
                           isa = ((isa >> 8) | ((isa << 8) & 0xFF00))
                        if isa in elf_isa:
                           if isa == 0 or elf_isa[isa] == output_isa:
                              check_passed = True
            elif magic_header[:2] == "MZ":
               link_file.seek(60)
               pe_offset = int(link_file.read(4).encode("hex"), 16)
               pe_offset = ((pe_offset >> 24) |
                           ((pe_offset & 0x00FF0000) >> 8) |
                           ((pe_offset & 0x0000FF00) << 8) |
                           ((pe_offset & 0x000000FF) << 24))
               link_file.seek(pe_offset)
               magic_header = link_file.read(4)
               if magic_header == "PE\x00\x00":
                  if not output_format or output_format == file_format[0]:
                     if not output_isa:
                        check_passed = True
                     else:
                        isa = int(link_file.read(2).encode("hex"), 16)
                        isa = ((isa >> 8) | ((isa << 8) & 0xFF00))
                        if isa in pe_isa:
                           if isa == 0 or pe_isa[isa] == output_isa:
                              check_passed = True
   except:
      pass
   finally:
      if link_file:
         link_file.close()

   context.Result(int(check_passed))
   return int(check_passed)


#
# Validate that the specified environment key (which is assumed to be a list of
# valid directories) contains the specified value (i.e. Is file in one of the
# listed directories?).
#
# INPUT : context - SCons Configure() test context
#         key - Key containing a list of directories to search for value in
#         value - Value to search for in each directory
# OUTPUT: [Return] - int(True) if key contains value, int(False) otherwise
#
def CheckDirContains(context, key, value):
   check_passed = False

   context.Message("Checking if '{0}' is present... ".format(value))

   if key in context.env:
      dirs = context.env[key]
      for d in dirs:
         files = next(os.walk(d))[2]
         for f in files:
            if f.find(value) != -1:
               check_passed = True
               break

   context.Result(int(check_passed))
   return int(check_passed)


#
# Validate that the component value matches the expected value.
#
# INPUT : context - SCons Configure() test context
#         component - Component to validate value
#         value - Expected value of component
# OUTPUT: [Return] - int(True) if key contains value, int(False) otherwise
#
def CheckComponentValue(context, component, value):
   context.Message("Checking if '{0}' is '{1}'... ".format(component, value))

   if ((component in context.env.Dictionary()) and
       (context.env[component] == value)):
         check_passed = True
   else:
         check_passed = False

   context.Result(int(check_passed))
   return int(check_passed)


#
# Class describing modifications made to a specified environment by a specified
# function. Only environment keys set (that are not filtered out by the
# optionally supplied filter function) are tracked..
#
class ConfExEnvironmentModifier:
   def __init__(self, modifier, key_filter = None):
      self._keys_modified = set()
      self._key_filter = key_filter
      self._hook_values = []

      self.__enable_hooks(True)

      # Execute environment modifier function (saving return value)
      self.ModifierResult = modifier()
      self.Modifications = tuple(self._keys_modified)

      self.__enable_hooks(False)

   def __enable_hooks(self, enable):
      # 'public' environment dictionary methods to hook
      # Note: Some (possibly relevant) public methods are not hooked, but this
      #       list is expected to capture the majority (ideally all) of the
      #       keys modified in expected usage scenarios of this class
      hooks = (("Environment.__setitem__", self.__key_set),
               ("Environment.Append", self.__keys_append),
               ("Environment.AppendUnique", self.__keys_append_unique),
               ("Environment.AppendENVPath", self.__key_append_env),
               ("Environment.Prepend", self.__keys_prepend),
               ("Environment.PrependUnique", self.__keys_prepend_unique),
               ("Environment.PrependENVPath", self.__key_prepend_env),
               ("Environment.Replace", self.__keys_replace))

      for hook in hooks:
         if enable:
            # Hook methods (to track keys modified)
            self._hook_values.append(eval(hook[0]))
            exec(hook[0] + " = self._hook(eval(hook[0]), hook[1])")
         else:
            # Remove hooks (restore original values)
            exec(hook[0] + " = self._hook_values.pop(0)")


   def __key_modified(self, key):
      if self._key_filter and self._key_filter(key):
         self._keys_modified.add(key)

   def __key_set(self, env_self, key, item):
      self.__key_modified(key)

   def __keys_append(self, env_self, **kw):
      for key in kw.keys():
         self.__key_modified(key)

   def __keys_append_unique(self, env_self, delete_existing = 0, **kw):
      for key in kw.keys():
         # Note: A unique check (that is explicitly expected from this method)
         #       is being skipped so as not to have to rely on the SCons
         #       implmentation of such. This may lead to false-positives for
         #       functionality using this class.
         self.__key_modified(key)

   def __key_append_env(self, env_self, name, newpath, envname = "ENV",
                        sep = ":", delete_existing = 1):
      self.__key_modified(envname)

   def __keys_prepend(self, env_self, **kw):
      for key in kw.keys():
         self.__key_modified(key)

   def __keys_prepend_unique(self, env_self, delete_existing = 0, **kw):
      for key in kw.keys():
         # Note: [See note in 'self.keysAppendUnique']
         self.__key_modified(key)

   def __key_prepend_env(self, env_self, name, newpath, envname = "ENV",
                         sep = ":", delete_existing = 1):
      self.__key_modified(envname)

   def __keys_replace(self, env_self, **kw):
      for key in kw.keys():
         self.__key_modified(key)

   def _hook(self, func, hookfunc):
       def hooked_func(*args, **kwargs):
           hookfunc(*args, **kwargs)
           return func(*args, **kwargs)
       return hooked_func


#
# Class describing a ConfExError exception (thrown when ConfExBase.Find()
# results in an error). See applicable method below.
#
class ConfExError(Exception):
   def __init__(self, error, ctx = None):
      self.Error = error
      self.Description = error
      self.Name = None

      if error == "ToolNotFound":
         self.Name = ctx
         self.Description = ("No tool found that provides a suitable '" +
                             self.Name + "'")
      elif error == "LibraryNotFound":
         self.Name = ctx
         self.Description = ("Library '" + self.Name + "' not found")
      elif error == "ProgramNotFound":
         self.Name = ctx
         self.Description = ("External program '" + self.Name + "' not found")

      super(ConfExError, self).__init__(self.Description)


#
# Class describing a valid environment augment specification.
#
class ConfExSpecification:
   def __init__(self, component, check, depends):
      # 'component' may be a single string (environment component) or a list of
      # strings (all of which will be checked individually, each as a component,
      # with one being chosen to be used).
      if component:
         try:
            _ = iter(component)
            if type(component) is str:
               raise TypeError()
         except TypeError:
            self.Components = tuple(OrderedDict.fromkeys((component,)))
         else:
            self.Components = tuple(OrderedDict.fromkeys(component))
      else:
         self.Components = None

      # 'check' may be a single function or a list of functions. Any checks
      # specified must pass for a component to be considered valid.
      if check:
         try:
            _ = iter(check)
         except TypeError:
            self.Checks = (check,)
         else:
            self.Checks = tuple(check)
      else:
         self.Checks = None

      # 'depends' may be a single spec object (a single component dependency) or
      # a list of spec objects (specifying multiple component dependencies)
      if depends:
         try:
            _ = iter(depends)
         except TypeError:
            self.Dependencies = (depends,)
         else:
            self.Dependencies = tuple(depends)
      else:
         self.Dependencies = None


#
# Class describing an environment augment (i.e. An augment specification that
# has been or will be fulfilled before being applied to the environment)
#
class ConfExEnvironmentAugment:
   def __init__(self, spec):
      self.Specification = spec
      self.Tool = None
      self.Component = None
      self.Valid = False


#
# Class that encapsulates a standard environment (allowing for caching and other
# optimizations).
#
class ConfExEnvironment:
   @staticmethod
   def _isComponentKey(key):
      is_component = True

      # Skip 'TOOLS' key, private keys and keys that end with 'PREFIX',
      # 'SUFFIX', 'FLAGS', 'COM' or 'VERSION'
      if (key == "TOOLS" or
         key.startswith("_") or
         not key.isupper() or
         key.endswith("PREFIX") or
         key.endswith("SUFFIX") or
         key.endswith("FLAGS") or
         key.endswith("COM") or
         key.endswith("VERSION")):
         is_component = False
      return is_component

   def __init__(self, env, reset_env, enable_config_h, disable_config_h):
      self.Tools = env["TOOLS"]
      self._base = env.Clone()
      self._current = env
      self._augments = []
      self._cache = ConfExCache.Get(self, self._base)
      self.__applied_tools = []
      self.__applied_checks = []
      self.__reset_env = reset_env
      self.__enable_config_h = enable_config_h
      self.__disable_config_h = disable_config_h

   def AddAugment(self, spec):
      added = False
      augments = self._clone_augments(self._augments)
      augment = ConfExEnvironmentAugment(spec)
      change = []
      changes = []

      ConfExDisplay("> Adding environment augment...")

      # Add augment to environment
      augments.append(augment)

      if spec.Components is None:
         do_loop = False
         if not self._validate_augments(augments):
            self._augments = augments
            added = True
      else:
         # Resolve component (to ensure it will be part of environment)
         ConfExDisplay("- Configuring augment component...")
         do_loop = self._set_component(augment)

      while (len(changes) or do_loop):
         # Skip (empty) change list on first iteration
         if do_loop:
            do_loop = False
         else:
            last_change = change
            change = changes.pop()
            change_delta = ([], [])

            # Get delta of last change and current change lists
            for change_offset in [(change, 1), (last_change, -1)]:
               offset = change_offset[1]
               for augment in change_offset[0]:
                  try:
                     # Try to increment change augment (if already present)
                     idx = change_delta[0].index(augment)
                     change_delta[1][idx] += offset

                     # Remove augments that resolve to being unchanged
                     if change_delta[1][idx] == 0:
                        del change_delta[0][idx]
                        del change_delta[1][idx]
                  except ValueError:
                     # Augment not present, add (with specified offset)
                     change_delta[0].append(augment)
                     change_delta[1].append(offset)

            # Apply change delta
            no_component = False
            for i in range(len(change_delta[0])):
               augment = change_delta[0][i]
               offset = change_delta[1][i]
               if not self._set_component(augment, offset):
                  no_component = True
                  break
            if no_component:
               # Update change to what was actually set
               change = list(last_change)
               for x in range(i):
                  augment = change_delta[0][x]
                  offset = change_delta[1][x]
                  for _ in range(abs(offset)):
                     if offset > 0:
                        change.append(augment)
                     else:
                        change.remove(augment)

               ConfExDisplay("- No alternate component, reconfiguring...")
               continue

         # Order augments (to ensure each is applied in the correct order)
         incompatible_augments = self._order_augments(augments)
         if incompatible_augments:
            for augment in incompatible_augments:
               new_change = list(change)
               new_change.append(augment)
               changes.append(new_change)

            ConfExDisplay("- Incompatible augment(s), reconfiguring...")
            continue

         # Validate augments (to ensure the underlying specification is met)
         invalid_augments = self._validate_augments(augments)
         if invalid_augments:
            #TODO: Add augment dependencies to changes (to start swapping
            #      dependency augments if invalid augment can't be changed)
            change = []
            changes = [invalid_augments]

            ConfExDisplay("- Invalid augment(s), reconfiguring...")
            continue

         self._augments = augments
         added = True
         break

      return added

   def Detect(self, tool):
      # Note: The below logic could use 'self._apply_env([tool], True)' to gain
      # the benefits that such functionality offers. However, a specifc "detect"
      # environment is being utilized below. The reasons for such are two-fold;
      # Firstly, a workaround (which requires a duplicate Tool application) is
      # being utilized, and second, this method is called to acertain a Tools
      # additions (of which some or none may not be applicable)

      args = {"tools" : [tool]}
      if self._cache.Toolpath:
         args["toolpath"] = self._cache.Toolpath

      # Record number of tools already in environment
      if self._base["TOOLS"]:
         orig_tools_len = len(self._base["TOOLS"])
      else:
         orig_tools_len = 0

      # Apply tool to environment (tracking tool modifications)
      mod = ConfExEnvironmentModifier(lambda : self._base.Clone(**args),
                                      ConfExEnvironment._isComponentKey)

      # Get tools/components added to environment
      env_clone = mod.ModifierResult
      tools = env_clone["TOOLS"]
      components = dict.fromkeys(mod.Modifications, ["no_overlap"])

      # WORKAROUND: Re-apply tool (to determine which components can overlap).
      #             Above also calls for exact apply to have single tool only
      del args["tools"]
      args["tool"] = tool
      mod = ConfExEnvironmentModifier(lambda : env_clone.Tool(**args),
                                      ConfExEnvironment._isComponentKey)

      # Remove flag from components that can overlap (other tool components)
      components.update(dict.fromkeys(mod.Modifications, []))

      # Return tool(s) and components detected
      return (tools[orig_tools_len:], components)

   def Finalize(self):
      self.__enable_config_h()
      tools = []
      checks = []

      # Add all augments tools and checks
      for augment in self._augments:
         if augment.Tool:
            tools.append(augment.Tool)
         if augment.Specification.Checks:
            checks.extend(augment.Specification.Checks)

      self._apply_env(tools, True, None, checks)

   def _clone_augments(self, augments):
      cloned_augments = []

      for augment in augments:
         new_augment = ConfExEnvironmentAugment(augment.Specification)
         new_augment.Component = augment.Component
         new_augment.Tool = augment.Tool
         new_augment.Valid = augment.Valid
         cloned_augments.append(new_augment)

      return cloned_augments

   def _set_component(self, augment, offset = 0):
      component_set = False
      offset = [offset]
      ref_component = augment.Component
      components = augment.Specification.Components

      if not augment.Tool:
         component = self.__set_component_local(components,
                                                ref_component,
                                                offset)
         if component:
            tool = None
            component_set = True
         else:
            if offset >= 0:
               ref_component = components[0]

      if not component_set:
         # Get a tool that provides specification component
         component_tool = self._cache.GetTool(components,
                                              ref_component,
                                              augment.Tool,
                                              offset)
         if component_tool:
            component = component_tool[0]
            tool = component_tool[1]
            component_set = True
         else:
            if offset <= 0:
               ref_component = components[len(components) - 1]
               component = self.__set_component_local(components,
                                                      ref_component,
                                                      offset)
               if component:
                  tool = None
                  component_set = True

      if component_set:
         augment.Component = component
         augment.Tool = tool
         augment.Valid = False

      return component_set

   def _order_augments(self, augments):
      ordered = []
      incompatibility = None

      # Reverse augment list order. If there are no actual re-orderings below
      # the resultant (ordered) list will be the same as the input (augment)
      # list (which, when augments are validated, allows for fewer environment
      # resets to occur, and thus less Tool and Check re-applications)
      augments.reverse()

      try:
         for augment in augments:
            active_idx = 0
            overlap_component = None

            if (augment.Tool and
                "no_overlap" in self._cache.Components[augment.Tool]
                                                      [augment.Component]):
                  overlap_component = augment

            # Find proper position for augment component to be "active"
            tool_present = False
            for i in range(len(ordered)):
               # Check if augments tool has already been specified
               if augment.Tool and (augment.Tool == ordered[i].Tool):
                  if not tool_present:
                     tool_present = True
                     active_idx = i
                  continue

               # Check if augment tool overlaps the current ordered component
               if (augment.Tool and
                  ordered[i].Component in self._cache.Components[augment.Tool]):
                  overlap_component = ordered[i]

               # Check if augment component is not active
               if (ordered[i].Tool and
                  augment.Component in self._cache.Components[ordered[i].Tool]):
                  if overlap_component or tool_present:
                     incompatibility = (augment, ordered[i])
                     raise ConfExError("Incompatible augments")
                  else:
                     active_idx = (i + 1)

            ordered.insert(active_idx, augment)
      except ConfExError:
         pass
      else:
         del augments[:]
         augments.extend(ordered)

      return incompatibility

   def _validate_augments(self, augments):
      self.__disable_config_h()
      invalid_augments = []
      tools = []
      checks = []
      depends_checks = []

      # Apply currently invalid augments to environment and run checks
      for augment in augments:
         dependency_augment = False
         if augment.Valid:
            for depends in augments:
               if (not depends.Valid and
                   depends.Specification.Dependencies and
                   augment.Specification in depends.Specification.Dependencies):
                  dependency_augment = True
                  break
            if not dependency_augment:
               continue

         if augment.Tool:
            tools.append(augment.Tool)
         if augment.Specification.Checks:
            if dependency_augment:
               depends_checks.extend(augment.Specification.Checks)
            else:
               checks.extend(augment.Specification.Checks)

      failed_checks = self._apply_env(tools, False, checks, depends_checks)

      # Mark invalid augments (where one or more checks failed)
      for augment in augments:
         augment.Valid = True
         if (failed_checks and
             augment.Specification.Checks):
            for check in failed_checks:
               if check in augment.Specification.Checks:
                  augment.Valid = False

         if not augment.Valid:
            invalid_augments.append(augment)

      return invalid_augments

   def _apply_env(self, tools, exact, req_checks = None, opt_checks = None):
      reset = False
      args = {}
      failed_checks = []
      expected = -1

      if self._cache.Toolpath:
         args["toolpath"] = self._cache.Toolpath

      tools = tuple(OrderedDict.fromkeys(tools))
      append = len(tools)
      if req_checks:
         req_checks = tuple(OrderedDict.fromkeys(req_checks))

      # If an exact application is required the environment may not have any
      # additional tools then what is specified by the provided tools list
      if exact:
         for tool in self.__applied_tools:
            if tool not in tools:
               reset = True
               break

      # Find starting index in tool list of tools that need to be applied (if
      # these tools can be applied to the current environment, i.e. appended)
      if not reset:
         for i in range(len(tools)):
            try:
               actual = self.__applied_tools.index(tools[i])
               if ((i == 0) and
                   ((len(tools) > 1) or
                   (actual == (len(self.__applied_tools) - 1)))):
                  expected = actual

               if actual == expected:
                  expected += 1
               else:
                  # Tool found, but out of order
                  reset = True
                  break
            except:
               # Check if first tool has not yet been found in applied list
               if expected == -1:
                  append = 0
               else:
                  # Check if all found previous tools were at end of list
                  if expected == len(self.__applied_tools):
                     # Set start of tools append index (if not already set)
                     if append == len(tools):
                        append = i
                  else:
                     reset = True
                     break

      if reset:
         ConfExDisplay("- Resetting environment...")
         self._current = self.__reset_env()
         self.__applied_tools = []
         self.__applied_checks = []
         start = 0
      else:
         start = append

      # Apply tools
      expected_len = (len(self._current["TOOLS"]) + 1)
      for i in range(start, len(tools)):
         args["tool"] = tools[i]
         self.__applied_tools.append(tools[i])
         self._current.Tool(**args)

         # Check if additional tools were added (and remove names if they were)
         actual_len = len(self._current["TOOLS"])
         if actual_len != expected_len:
            self._current["TOOLS"] = self._current["TOOLS"][:expected_len]

         expected_len += 1

      # Run checks
      ConfExDisplay("- Validating environment augments...")
      all_checks = ((req_checks, True), (opt_checks, False))
      for checks_type in all_checks:
         checks = checks_type[0]
         required = checks_type[1]
         if checks:
            for check in checks:
               if check not in self.__applied_checks:
                  self.__applied_checks.append(check)
               else:
                  if not required:
                     continue

               try:
                  if not check():
                     raise Exception()
               except:
                  failed_checks.append(check)

      return failed_checks

   def __set_component_local(self, components, ref_component, offset):
      component = None
      forward = True if (offset[0] >= 0) else False

      if ref_component:
         component_idx = components.index(ref_component)
      else:
         component_idx = 0

      while ((component_idx >= 0) and
             (component_idx < len(components))):
         if (components[component_idx] in self._base.Dictionary()):
            if offset[0] < 0:
               offset[0] += 1
            elif offset[0] > 0:
               offset[0] -= 1
            else:
               component = components[component_idx]
               break

         if forward:
            component_idx += 1
         else:
            component_idx -= 1

      return component


#
# Class describing a cache of tools configured for use with a specific
# environment
#
class ConfExCache:
   _EnvironmentCache = []
   Tools = None
   Toolpath = None

   @staticmethod
   def Get(env, ref_key):
      found_cache = None

      for key, cache in ConfExCache._EnvironmentCache:
         if key == ref_key:
            found_cache
            break

      if found_cache is None:
         cache = ConfExCache(env)
         ConfExCache._EnvironmentCache.append((ref_key, cache))
      else:
         cache = found_cache

      return cache

   def __init__(self, env):
      self._env = env
      self.Tools = list(ConfExCache.Tools)
      self.Toolpath = tuple(ConfExCache.Toolpath)
      self.Components = {}

      if env.Tools:
         self.Tools += list(env.Tools)

      self.Tools = list(OrderedDict.fromkeys(self.Tools))

      if "ConfigureEx" in self.Tools:
         # Remove 'ConfigureEx' tool (reinitializing this module while
         # running causes issues)
         self.Tools.remove("ConfigureEx")

   def AddTool(self, tool):
      if tool not in self.Tools:
         self.Tools.append(tool)

      tools_stack = [[tool]]
      tool_idx = self.Tools.index(tool)
      offset = 0

      while len(tools_stack):
         tools = tools_stack.pop(0)
         tool_idx -= offset

         while len(tools):
            tool = tools.pop(0)

            try:
               orig_idx = self.Tools.index(tool)
               if orig_idx > tool_idx:
                  # Remove tool at current position and re-insert
                  del self.Tools[orig_idx]
                  raise ValueError()
            except:
               # Insert newly identified tool
               self.Tools.insert(tool_idx, tool)

            if tool not in self.Components:
               # Get tool components and tools (if tool is in fact a toolchain)
               toolchain, components = self._env.Detect(tool)
               self.Components[tool] = components

               if len(toolchain) > 1:
                  # Save tools on stack to process after
                  tools_stack.insert(0, tools)

                  # Process new toolchain
                  tools = toolchain
                  offset = len(tools)

   def GetTool(self, components, ref_component, ref_tool, offset):
      tool_set = None
      forward = True if (offset[0] >= 0) else False

      if len(self.Tools):
         # Set starting values
         if ref_component:
            component_idx = components.index(ref_component)
         else:
            component_idx = 0
         if ref_tool:
            tool_idx = self.Tools.index(ref_tool)
         else:
            tool_idx = 0

         # Cycle through tools
         while not tool_set:
            if ((tool_idx < 0) or
                (tool_idx >= len(self.Tools))):
               break

            tool = self.Tools[tool_idx]
            if tool not in self.Components:
               # Add unused/unknown tool to cache
               self.AddTool(tool)
               continue

            # Cycle through components
            while ((component_idx >= 0) and
                   (component_idx < len(components))):
               component = components[component_idx]
               if component in self.Components[tool]:
                  # Skip 'offset' number of matches
                  if offset[0] < 0:
                     offset[0] += 1
                  elif offset[0] > 0:
                     offset[0] -= 1
                  else:
                     tool_set = (component, tool)
                     break

               if forward:
                  component_idx += 1
               else:
                  component_idx -= 1

            if forward:
               tool_idx += 1
               component_idx = 0
            else:
               tool_idx -= 1
               component_idx = (len(components) - 1)

      return tool_set


#
# Subclass of SConfBase (object returned by env.ConfigureEx())
#
class ConfExBase(SConfBase):
   def __init__(self, env, custom_tests, conf_dir, log_file, config_h, listing,
                silent):
      # Add default custom tests
      tests = {"CheckLink" : CheckLink,
               "CheckDirContains" : CheckDirContains,
               "CheckComponentValue" : CheckComponentValue}
      tests.update(custom_tests)
      conf_env = env.Clone()

      # Initialize superclass
      super(ConfExBase, self).__init__(conf_env, tests, conf_dir, log_file,
                                       config_h)

      # Cloned here (instead of on lambda definition) as lambda function
      # arguments bind late, i.e. reference now, value later)
      conf_clone = conf_env.Clone()

      self.__listing = listing
      self.__silent = silent
      self.__config_h = config_h
      self.__env = ConfExEnvironment(conf_env,
                                  lambda : self.__reset_environment(conf_clone),
                                  lambda : self.__enable_config_h(),
                                  lambda : self.__disable_config_h())

   def FindComponent(self, component, check = None, name = None,
                     depends = None):
      if not component:
         raise ValueError("Component not specified")

      if not name:
         name = str(component)

      if not self.__is_listing("COMPONENT", name):
         spec = self.__wrap(lambda : self.__find(component, check, depends))
         if not spec:
            raise ConfExError("ToolNotFound", name)
         else:
            return spec

   def FindLibrary(self, name, check = None, depends = None):
      if not name:
         raise ValueError("Library name not specified")

      if not self.__is_listing("LIBRARY", name):
         if not check:
            component = "LIBPATH"
            check = [lambda : self.CheckDirContains(component, name)]
         else:
            component = None
            try:
               _ = iter(check)
            except TypeError:
               check = (check,)

         spec = self.__wrap(lambda : self.__find(component, check, depends))
         if not spec:
            raise ConfExError("LibraryNotFound", name)
         else:
            return spec

   def FindProgram(self, name, check = None):
      if not name:
         raise ValueError("Program name not specified")

      if not self.__is_listing("PROGRAM", name):
         checks = [lambda : self.CheckProg(name)]
         if check:
            try:
               _ = iter(check)
               checks.extend(check)
            except TypeError:
               checks.append(check)

         spec = self.__wrap(lambda : self.__find("ENV", checks))
         if not spec:
            raise ConfExError("ProgramNotFound", name)
         else:
            return spec

   def Finish(self):
      return self.__wrap(lambda : self.__finish())

   def __wrap(self, func):
      global progress_display
      global ConfExDisplay

      # Disable output if running silent
      if self.__silent:
         local_display = ConfExDisplay
         super_display = progress_display
         ConfExDisplay = lambda msg: None
         SetProgressDisplay(self.__nullDisplay)

      result = func()

      # Restore output
      if self.__silent:
         ConfExDisplay = local_display
         SetProgressDisplay(super_display)

      return result

   def __find(self, component, check = None, depends = None):
      # Component specification
      spec = ConfExSpecification(component, check, depends)

      # Add new environment augment (to provide component)
      if not self.__env.AddAugment(spec):
         spec = None

      return spec

   def __finish(self):
      # Finalize environment (ensure all tools are applied)
      self.__env.Finalize()

      # Finish superclass
      return super(ConfExBase, self).Finish()

   def __is_listing(self, class_type, name):
      if not (self.__listing is None):
         if class_type in self.__listing:
            self.__listing[class_type].append(name)
         else:
            self.__listing[class_type] = [name]

      return (False if (self.__listing is None) else True)

   def __reset_environment(self, env):
      # Reset base Configure() environment to original
      self.env = env.Clone()
      return self.env

   def __enable_config_h(self):
      # Enable config_h property (allowing tests to modify it)
      self.config_h = self.__config_h

   def __disable_config_h(self):
      # Disable config_h property (preventing tests from modifying it)
      self.__config_h = self.config_h
      self.config_h = None

   def __nullDisplay(self, output, append_newline):
      # No output, i.e. sinking all output nowhere
      pass


def CacheTools(env, tools = None, toolpath = None):
   if tools:
      try:
         _ = iter(tools)
         if type(tools) is str:
            raise TypeError()
      except TypeError:
         tools = (tools,)

      try:
         _ = iter(toolpath)
         if type(toolpath) is str:
            raise TypeError()
      except TypeError:
         toolpath = (toolpath,)

   ConfExCache.Tools = tools
   ConfExCache.Toolpath = toolpath


def ConfigureEx(env,
                custom_tests = {},
                conf_dir = "$CONFIGUREDIR",
                log_file = "$CONFIGURELOG",
                config_h = None,
                listing = None,
                silent = True):
   # Create ConfigureEx base object
   return ConfExBase(env, custom_tests, conf_dir, log_file, config_h, listing,
                     silent)


def generate(env):
   env.AddMethod(CacheTools, "CacheTools")
   env.AddMethod(ConfigureEx, "ConfigureEx")


def exists(env):
   return True
