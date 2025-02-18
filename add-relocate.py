import lief

python = lief.parse('python')
environ_symtab = python.get_symtab_symbol('_z_import_environ')

environ_dynamic = lief.ELF.Symbol()
environ_dynamic.name = 'environ'
environ_dynamic.imported = True
environ_dynamic.binding = lief.ELF.Symbol.BINDING.GLOBAL
environ_dynamic.type = lief.ELF.Symbol.TYPE.OBJECT
python.add_dynamic_symbol(environ_dynamic)

environ_dynamic_reloc = lief.ELF.Relocation(
    environ_symtab.value, type=lief.ELF.Relocation.TYPE.X86_64_64,
    encoding=lief.ELF.Relocation.ENCODING.RELA)
environ_dynamic_reloc.symbol = environ_dynamic
python.add_dynamic_relocation(environ_dynamic_reloc)

python.write('python.edit')
