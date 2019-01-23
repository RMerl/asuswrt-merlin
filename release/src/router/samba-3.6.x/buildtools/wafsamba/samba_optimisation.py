# This file contains waf optimisations for Samba

# most of these optimisations are possible because of the restricted build environment
# that Samba has. For example, Samba doesn't attempt to cope with Win32 paths during the
# build, and Samba doesn't need build varients

# overall this makes some build tasks quite a bit faster

from TaskGen import feature, after
import preproc, Task

@feature('cc', 'cxx')
@after('apply_type_vars', 'apply_lib_vars', 'apply_core')
def apply_incpaths(self):
	lst = []

	try:
		kak = self.bld.kak
	except AttributeError:
		kak = self.bld.kak = {}

	# TODO move the uselib processing out of here
	for lib in self.to_list(self.uselib):
		for path in self.env['CPPPATH_' + lib]:
			if not path in lst:
				lst.append(path)
	if preproc.go_absolute:
		for path in preproc.standard_includes:
			if not path in lst:
				lst.append(path)

	for path in self.to_list(self.includes):
		if not path in lst:
			if preproc.go_absolute or path[0] != '/': #os.path.isabs(path):
				lst.append(path)
			else:
				self.env.prepend_value('CPPPATH', path)

	for path in lst:
		node = None
		if path[0] == '/': # os.path.isabs(path):
			if preproc.go_absolute:
				node = self.bld.root.find_dir(path)
		elif path[0] == '#':
			node = self.bld.srcnode
			if len(path) > 1:
				try:
					node = kak[path]
				except KeyError:
					kak[path] = node = node.find_dir(path[1:])
		else:
			try:
				node = kak[(self.path.id, path)]
			except KeyError:
				kak[(self.path.id, path)] = node = self.path.find_dir(path)

		if node:
			self.env.append_value('INC_PATHS', node)

@feature('cc')
@after('apply_incpaths')
def apply_obj_vars_cc(self):
    """after apply_incpaths for INC_PATHS"""
    env = self.env
    app = env.append_unique
    cpppath_st = env['CPPPATH_ST']

    lss = env['_CCINCFLAGS']

    try:
         cac = self.bld.cac
    except AttributeError:
         cac = self.bld.cac = {}

    # local flags come first
    # set the user-defined includes paths
    for i in env['INC_PATHS']:

        try:
            lss.extend(cac[i.id])
        except KeyError:

            cac[i.id] = [cpppath_st % i.bldpath(env), cpppath_st % i.srcpath(env)]
            lss.extend(cac[i.id])

    env['_CCINCFLAGS'] = lss
    # set the library include paths
    for i in env['CPPPATH']:
        app('_CCINCFLAGS', cpppath_st % i)

import Node, Environment

def vari(self):
	return "default"
Environment.Environment.variant = vari

def variant(self, env):
	if not env: return 0
	elif self.id & 3 == Node.FILE: return 0
	else: return "default"
Node.Node.variant = variant


import TaskGen, Task

def create_task(self, name, src=None, tgt=None):
    task = Task.TaskBase.classes[name](self.env, generator=self)
    if src:
        task.set_inputs(src)
    if tgt:
        task.set_outputs(tgt)
    return task
TaskGen.task_gen.create_task = create_task

def hash_constraints(self):
	a = self.attr
	sum = hash((str(a('before', '')),
            str(a('after', '')),
            str(a('ext_in', '')),
            str(a('ext_out', '')),
            self.__class__.maxjobs))
	return sum
Task.TaskBase.hash_constraints = hash_constraints


# import cc
# from TaskGen import extension
# import Utils

# @extension(cc.EXT_CC)
# def c_hook(self, node):
# 	task = self.create_task('cc', node, node.change_ext('.o'))
# 	try:
# 		self.compiled_tasks.append(task)
# 	except AttributeError:
# 		raise Utils.WafError('Have you forgotten to set the feature "cc" on %s?' % str(self))

# 	bld = self.bld
# 	try:
# 		dc = bld.dc
# 	except AttributeError:
# 		dc = bld.dc = {}

# 	if task.outputs[0].id in dc:
# 		raise Utils.WafError('Samba, you are doing it wrong %r %s %s' % (task.outputs, task.generator, dc[task.outputs[0].id].generator))
# 	else:
# 		dc[task.outputs[0].id] = task

# 	return task


def suncc_wrap(cls):
	'''work around a problem with cc on solaris not handling module aliases
	which have empty libs'''
	if getattr(cls, 'solaris_wrap', False):
		return
	cls.solaris_wrap = True
	oldrun = cls.run
	def run(self):
		if self.env.CC_NAME == "sun" and not self.inputs:
			self.env = self.env.copy()
			self.env.append_value('LINKFLAGS', '-')
		return oldrun(self)
	cls.run = run
suncc_wrap(Task.TaskBase.classes['cc_link'])
