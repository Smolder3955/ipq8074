#@PydevCodeAnalysisIgnore

'''
===============================================================================

 Update Common Info

 GENERAL DESCRIPTION
    This script performs task that are common across all builds, and which
    must be done after all builds are complete.

 Copyright (c) 2011-2012 by QUALCOMM, Incorporated.
 All Rights Reserved.
 QUALCOMM Proprietary/GTDR

-------------------------------------------------------------------------------

  $Header: //depot/asic/ipq8066/IPQ8074.ILQ.10.0/meta/main/latest/common/build/update_common_info.py#31 $
  $DateTime: 2019/05/06 06:20:51 $

-------------------------------------------------------------------------------

update_common_info.py:

Usage:

      update_common_info.py <mode>

   Examples:

      update_common_info.py --nonhlos  (generates NON_HLOS.bin alone)
      update_common_info.py --hlos     (generates sparse images if rawprogram0.xml exists)
      update_common_info.py            (generates NON-HLOS.bin and sparse images)

===============================================================================
'''

import sys
import os
import distutils.core
import os.path
import shutil
import fnmatch
# Hack until modules deliver split files
import stat
import string
import subprocess
from optparse import OptionParser
from glob import glob

sys.path.append('../tools/meta')
import meta_lib as ml
import meta_log as lg
from xml.etree import ElementTree as et
bin_file_dests = dict()
profile=os.environ['BLD_ENV_BUILD_ID']

class ModemPartitionSizeNotFoundException( Exception ):
   '''An exception class for modem partition size not found.'''
   def __init__(self, list_invalid_files ):
      Exception.__init__(self)

#function to read the value of modem partition size from partition.xml
def getModemPartitionSize():
   '''This function reads the value of modem partition size from partition.xml
   If not found it raises an exception
   '''
   modem_partition_size = 0

   inputXmlTree = et.parse( 'partition.xml' )
   root_element = inputXmlTree.getroot()

   #get the "physical_partition" element
   list_physical_partition_elements = root_element.findall( 'physical_partition')

   for physical_partition_el in list_physical_partition_elements:
      list_partition = physical_partition_el.findall('partition')
      for list_partition_el in list_partition:
         if( list_partition_el.get('label') == 'modem' ):
            modem_partition_size = int( list_partition_el.get('size_in_kb') )
            modem_partition_size = ( modem_partition_size + 1023 ) / 1024 #change the size from KB to MB and ensure that if necessary, 1 MB extra is allocated
            return modem_partition_size

   raise ModemPartitionSizeNotFoundException()
#end of function getModemPartitionSize()

#----------------------------------------------------------------------------------#
#creating gen_buildflavor.cmm file in common/tools/cmm/gen directory --------------#
#----------------------------------------------------------------------------------#
import meta_lib as ml
destn_dir  = '../tools/cmm/gen'

if not os.path.exists(destn_dir):
   os.makedirs(destn_dir)
mi = ml.meta_info(os.path.join(os.path.dirname(__file__), '../../contents.xml'))
cmm_script = open(os.path.join(os.path.dirname(__file__), '../tools/cmm/gen/gen_buildflavor.cmm'), 'w')
var_values = mi.get_var_values('cmm_var')
cmm_script_trailer = '''\nENDDO'''

cmm_script.write("ENTRY &ARG0\n\n")
flavors_list = mi.get_product_flavors()
if flavors_list:
   default_flavor = flavors_list[0]
else:
   default_flavor = "None"
   flavors_list.append(default_flavor)
    
file_vars = mi.get_file_vars(attr='cmm_file_var', abs=False) 
for var_name in file_vars:
     cmm_script.write("GLOBAL &"+var_name+"\n")
 
for flavor1 in flavors_list:
     # Look for cmm_file_var attributes:
     cmm_script.write("\nIF (\"&ARG0\"==\""+flavor1+"\")\n(\n")
     file_vars = mi.get_file_vars(attr='cmm_file_var', flavor=flavor1, abs=False)
     cmm_script.write("\t&PRODUCT_FLAVOR=\""+flavor1+"\"\n")
     for var_name in file_vars:
         file_list = file_vars[var_name]
         for file in file_list:
             if string.find(file,'LINUX')!=-1 and string.find(file,'android')!=-1 :
                dirs=string.split(file,os.sep)
                file = string.join(dirs[2:],os.sep)
             cmm_file_array = ''
             cmm_file_array += file + ';'
             cmm_file_array = cmm_file_array[:-1]
             cmm_script.write("\t&"+var_name+"=\""+cmm_file_array+"\"\n")
     cmm_script.write(")")        
cmm_script.write(cmm_script_trailer)
cmm_script.close()

#-------------------------------------------------------------------------------------------------------------------------#
#--------------This function creates partition ---------------------------------------------------------------------------#
#-------------------------------------------------------------------------------------------------------------------------#   
def  use_fatgen_toget_partitionsize ():
   if os.path.exists( fatgen_tool ):
      lg.log('fatgen tool exists, using it to create new NON-HLOS.bin')
         #get modem partition size from partition.xml
      try:
         modem_partition_size = getModemPartitionSize()
         str_modem_partition_size = str( modem_partition_size )
         #print str_modem_partition_size
         
      except ModemPartitionSizeNotFoundException , ex:
         sys.exit('ModemPartitionSizeNotFoundException raised')
   else:
      sys.exit('fatgen tool does not exist.')
   return str_modem_partition_size

def initialize ():
   if flavors:
      lg.log('flavors found')
      for flavor in flavors:
         path = os.path.join('./bin/', flavor)
         bin_file_dests[flavor] = path
         try:
            os.makedirs(path)
         except:
            # Ignore the exception that gets raised if the directory already exists.
            pass
   else:
      bin_file_dests[None] = ''

#-------------------------------------------------------------------------------------------------------------------------#
#--------------use cpfatfs tool to add binares---------------------------------------------------------------------------#
#-------------------------------------------------------------------------------------------------------------------------#   

def use_cpfatfs():
   
   for flavor in bin_file_dests:
      fat_file_dest = os.path.join(bin_file_dests[flavor], 'NON-HLOS.bin')
      
      #if NON-HLOS.bin exists delete it, a new NON-HLOS.bin will be created
      #if NON-HLOS.bin already exists, fatgen.py doesn't create it
      #deleting this file so that it is created each time
      if os.path.exists( fat_file_dest ):
         os.remove( fat_file_dest )
         lg.log("Existing " + fat_file_dest + " has been deleted.") 
      if (fatgen_build):
         # now create NON-HLOS.bin of container size as specifief in partition.xml
         modem_partition_size = use_fatgen_toget_partitionsize ()
         lg.log_exec(['python',fatgen_tool,'-f','-s',modem_partition_size,'-n',fat_file_dest],verbose=0)
      else:
         # Copy fat.bin to our root directory.  Assume fat.bin is in same directory as cpfatfs.exe   
         shutil.copy(cpfatfs_path + fat_file_src, fat_file_dest)   
         
   
      os.chmod(fat_file_dest, stat.S_IREAD | stat.S_IWRITE | stat.S_IRGRP | stat.S_IWGRP | stat.S_IROTH | stat.S_IWOTH)
      fat_dest_dir = 'image'
      for fat_file in mi.get_files(attr = 'fat_file', flavor = flavor):
         lg.log('update_common_info.py:' + 'cpfatfs' + ' ' +  fat_file +  ' ' + fat_file_dest,verbose=0)
         lg.log_exec([cpfatfs_tool, fat_file_dest, fat_dest_dir, fat_file],verbose=0)
#---------------------------------------------End of use_cpfatfs function----------------------------------------------------------------------------#   


#-------------------------------------------------------------------------------------------------------------------------#
#--------------use fat add tool to add binares---------------------------------------------------------------------------#
#-------------------------------------------------------------------------------------------------------------------------#   
       
def   use_fat_add ():
              
   for flavor in bin_file_dests:
      fat_file_dest = os.path.join(bin_file_dests[flavor], 'NON-HLOS.bin')

      #if NON-HLOS.bin exists delete it, a new NON-HLOS.bin will be created
      #if NON-HLOS.bin already exists, fatgen.py doesn't create it
      #deleting this file so that it is created each time
      if os.path.exists( fat_file_dest ):
         os.remove( fat_file_dest )
         lg.log("Existing " + fat_file_dest + " has been deleted.")
         # now create NON-HLOS.bin of container size as specifief in partition.xml
      if (fatgen_build):
         modem_partition_size = use_fatgen_toget_partitionsize ()
         lg.log_exec(['python',fatgen_tool,'-f','-s',modem_partition_size,'-n',fat_file_dest],verbose=0)
      else:
         # Copy fat.bin to our root directory.  Assume fat.bin is in same directory as cpfatfs.exe   
         shutil.copy(cpfatfs_path + fat_file_src, fat_file_dest)
               
      os.chmod(fat_file_dest, stat.S_IREAD | stat.S_IWRITE | stat.S_IRGRP | stat.S_IWGRP | stat.S_IROTH | stat.S_IWOTH)
      fat_dest_dir = 'image'
      for fat_file in mi.get_files(build='common', attr = 'fat_file', flavor=flavor):
         print fat_file
         lg.log('update_common_info.py:' + 'fatadd' + ' ' +  fat_file +  ' ' + fat_file_dest,verbose=0)
         lg.log_exec(['python',fatadd_tool,'-n',fat_file_dest,'-f' + fat_file,'-d'+ fat_dest_dir],verbose=0)
#-----------------------------------------------------------End of use_fat_add function--------------------------------------------------------------#
parser = OptionParser()
parser.add_option("--nonhlos",action="store_true",dest="non_hlos",help="NON HLOS",default=False)
parser.add_option("--hlos",action="store_true",dest="hlos",help="HLOS",default=False)
(options, args) = parser.parse_args()

FIT_tool = mi.get_tool_build('create_FIT_image')
#EXP_tools = mi.get_tool_build('exported_tools')
EXP_packages = mi.get_tool_build('exported_packages')
EXP_packages_64 = mi.get_tool_build('exported_packages_64')
META_TOOLS = mi.get_tool_build('meta_tools')
JTAG_SCRIPTS = mi.get_tool_build('jtag-scripts')
SKALES_tools = mi.get_tool_build('skales')

if not options.non_hlos and not options.hlos and FIT_tool is None:
   options.non_hlos = True
   options.hlos = True

#---------------------------------------------------------#
# Get bearings                                            #
#---------------------------------------------------------#

on_linux = sys.platform.startswith("linux")

#---------------------------------------------------------#
# Print some diagnostic info                              #
#---------------------------------------------------------#
lg = lg.Logger('update_common')
lg.log("update_common_info:Platform is:" + sys.platform,verbose=0)
lg.log("update_common_info:Python Version is:" + sys.version,verbose=0)
lg.log("update_common_info:Current directory is:" + os.getcwd(),verbose=0)

#---------------------------------------------------------#
# Load the Meta-Info file                                 #
#---------------------------------------------------------#
lg.log("update_common_info:Loading the Meta-Info file",verbose=0)
mi = ml.meta_info(logger=lg)

#---------------------------------------------------------#
# Get flavor information                                  #
#---------------------------------------------------------#
flavors = mi.get_product_flavors()

#---------------------------------------------------------#
# Setup our bin directory                                 #
#---------------------------------------------------------#
initialize()

#---------------------------------------------------------#
# Run PIL-Splitter                                        #
#---------------------------------------------------------#
pil_splitter_tool_name = 'pil-splitter.py'
pil_splitter_build  = mi.get_tool_build(pil_splitter_tool_name)
if pil_splitter_build:
   pil_splitter = os.path.join(mi.get_build_path(pil_splitter_build),
                               mi.get_tool_path(pil_splitter_tool_name),
                               pil_splitter_tool_name)
   pil_splitter = pil_splitter.replace('\\', '/')

   dest_dir  = '../tools/misc'
   if os.path.exists(pil_splitter):
      # Copy it into the meta-build, and run it from there.
      if not os.path.exists(dest_dir):
         os.mkdir(dest_dir)
      lg.log("Copying pil-splitter from host build.")
      shutil.copy(pil_splitter, dest_dir)

   # Always use local copy of pil_splitter
   pil_splitter = os.path.join (dest_dir, pil_splitter_tool_name)
   
   if flavors:
      for flavor in flavors:
         # Pil-Splitter requires a source file, and target prefix as parameters
         # The target prefix can include path information.  Files in contents.xml
         # that need to be split will have a 'pil-split' attribute set to the
         # prefix to use for that file.  When we call get_file_vars, it will
         # return a dictionary mapping the prefix to the file to be split.
         # We must then prepend any path information to the prefix.
         pil_split_bins_dir = os.path.join('bin', flavor, 'pil_split_bins')
         if not os.path.exists(pil_split_bins_dir):
            os.makedirs(pil_split_bins_dir)
         pil_splitter_files = mi.get_file_vars(attr = 'pil_split', flavor = flavor)
         for prefix in pil_splitter_files.keys():
            prefix_path = os.path.join(pil_split_bins_dir, prefix)
            # There should only be one source file per prefix
            pil_split_src = pil_splitter_files[prefix][0]
            lg.log('update_common_info: Calling pil-splitter on ' + pil_split_src)
            lg.log_exec(['python', pil_splitter, pil_split_src, prefix_path])
   else:
      pil_split_bins_dir = os.path.join('bin', 'pil_split_bins')
      if not os.path.exists(pil_split_bins_dir):
         os.makedirs(pil_split_bins_dir)
      pil_splitter_files = mi.get_file_vars(attr = 'pil_split')
      for prefix in pil_splitter_files.keys():
         prefix_path = os.path.join(pil_split_bins_dir, prefix)
         # There should only be one source file per prefix
         pil_split_src = pil_splitter_files[prefix][0]
         lg.log('update_common_info: Calling pil-splitter on ' + pil_split_src)
         lg.log_exec(['python', pil_splitter, pil_split_src, prefix_path])

#---------------------------------------------------------#
# Write the FAT image                                     #
#---------------------------------------------------------#
if options.non_hlos:
   lg.log("update_common_info:Writing FAT images")

   fatfiles = []

# check for fat file exist or not.
   if flavors:
      # if flavors exists, we pass one flavor element to mi.get_files() 
      # this is done just to avoid DataIntegrity exception
      # because we are only checking if there is atleast one file type element with 'fat_file' attribute
      fatfiles = mi.get_files(attr = 'fat_file', flavor = flavors[0] )
   else:
      fatfiles = mi.get_files(attr = 'fat_file')
   
   fat_file_src  = 'fat.bin'
   if len(fatfiles) > 0:
      try:
         #cheking if fat add tool exists or not
         fatadd_build  = mi.get_tool_build('fatadd')
         fatadd_path   = mi.get_build_path(fatadd_build)
         fatadd_path  += mi.get_tool_path('fatadd')
         fatadd_tool  =  fatadd_path + 'fatadd.py'
         fatbin_exists = False
         temp_path = os.path.abspath( os.path.join( fatadd_path , 'fat.bin' ) )
         if os.path.exists( temp_path ):
            fatbin_exists = True
      except:
         pass
         fatadd_build = False

      try:
         #checking if fatgen tool exists or not
         fatgen_build  = mi.get_tool_build('fatgen')
         fatgen_path   = mi.get_build_path( fatgen_build )
         fatgen_path  += mi.get_tool_path('fatgen')
         fatgen_tool  =  os.path.join(fatgen_path, 'fatgen.py')
      except:
         pass 
         fatgen_build = False

      try:
         #checking if cpfatfs tool exists or not
         cpfatfs_build  = mi.get_tool_build('cpfatfs')
         if cpfatfs_build:
            cpfatfs_path   = mi.get_build_path(cpfatfs_build)
            cpfatfs_path  += mi.get_tool_path('cpfatfs')
            cpfatfs_tool   = cpfatfs_path + 'cpfatfs'
            fatbin_exists = False
            temp_path = os.path.abspath( os.path.join( cpfatfs_path , 'fat.bin' ) )
            if os.path.exists( temp_path ):
               fatbin_exists = True
         
            if not on_linux:
               cpfatfs_tool += '.exe'
   
      except:
         pass 
         cpfatfs_build = False   
  
      if(fatgen_build or fatbin_exists) and (fatadd_build or cpfatfs_build):
         if (fatadd_build):
            use_fat_add()
         elif (cpfatfs_build):
            use_cpfatfs ()  
         else:
            sys.exit('No tool found to add component binaries to NON-HLOS.bin.')
      else:
         sys.exit('Unable to generate / process NON-HLOS.bin.')
      
      
   #---------------------------------------------------------#
   # Encode NON-HLOS.build                                   #
   #---------------------------------------------------------#
   try :
      ptool = mi.get_tool_build('enc_img')
   except :
      pass
      ptool = ''
   if ptool:  # Returns a build
      run_tool = True
      # Get the root path to the build
      enc_img_build_root = mi.get_build_path('tz')
      # Get rel path to tool
      enc_img_tool_path = mi.get_tool_path('enc_img')
      # Start processing parameters
      params = mi.get_tool_params('enc_img')
      for i in range(0, len(params)):
         if params[i].startswith("--tools_dir="):
            params[i] = params[i] % enc_img_build_root
      
         elif params[i].startswith("--keys="):
            key_dir = params[i].split('=')[1]
            if not key_dir:
               run_tool = False
   
      if run_tool:
         lg.log("Running Encoder on NON-HLOS.bin")
         cmd = ['python', os.path.join(enc_img_build_root, enc_img_tool_path, "enc_img.py"), "--ip_file=NON-HLOS.bin", "--op_file=NON-HLOS.enc"] + params
         lg.log_exec(cmd)
   
#---------------------------------------------------------#
# Process partition.xml                                   #
# Only run ptool at build time.  msp is run at download   #
# time.                                                   #
#---------------------------------------------------------#

   lg.log("update_common_info.py: Processing partition.xml",verbose=0)
   # Find the tools
   ptool_build  = mi.get_tool_build('ptool')
   if ptool_build:
      ptool_path   = mi.get_build_path(ptool_build)
      ptool_path  += mi.get_tool_path('ptool')
      ptool_path   = os.path.abspath(ptool_path)
      ptool_tool   = ptool_path + '/ptool.py'
      ptool_params = mi.get_tool_params('ptool')

      # Define our file names
      partition_xml   = 'partition.xml'
      rawprogram0_xml = 'rawprogram0.xml'

      if ptool_params:
         for i in range(len(ptool_params)):
            ptool_params[i] = ptool_params[i] % partition_xml   # Replace %s with path\name of partition.xml file
            ptool_params[i] = ptool_params[i].split()             # Split the command line into a list of arguments
      else:
         ptool_params = [['-x', partition_xml]]

      # Execute the commands
      for params in ptool_params:
         lg.log('update_common_info:' + ' ' +  'ptool.py'+ ' ' + partition_xml + ' ' + 'gpt',verbose=0)
         lg.log_exec(['python', ptool_tool] + params,verbose=0)

   nand_build  = mi.get_tool_build('nand_mbn_generator')
   if nand_build:
      # Define our file names
      partition_xml   = 'partition_nand.xml'
      rawprogram0_xml = 'rawprogram0.xml'
      partition_mbn = 'partition.mbn'
      nand_path   = mi.get_build_path(nand_build)
      nand_path  += mi.get_tool_path('nand_mbn_generator')
      nand_path   = os.path.abspath(nand_path)
      if os.path.exists(nand_path):
         nand_tool   = nand_path + '/nand_mbn_generator.py'
         nand_partition_xml=nand_path+'/partition_nand.xml'
         print nand_partition_xml
         lg.log('update_common_info:' + ' ' +  'nand_mbn_generator.py'+ ' ' + partition_xml + ' ' + 'gpt',verbose=0)
         lg.log_exec(['python', nand_tool,partition_xml,partition_mbn],verbose=0)

if options.hlos:

#---------------------------------------------------------#
# Run checksparse, if available.                          #
#---------------------------------------------------------#

   start_dir = os.getcwd()
   rawprogram_unsparse_xml = 'rawprogram_unsparse.xml'
   raw_program_xml = os.path.join(start_dir, 'rawprogram0.xml')
   if os.path.exists(raw_program_xml):
      for flavor in bin_file_dests:
         sparse_image_dest = os.path.join(bin_file_dests[flavor], 'sparse_images')
         sparse_image_info = mi.get_file_vars(attr='sparse_image_path')
         if sparse_image_info and len(sparse_image_info['true']):
            # There should only be one file, and the attr value is 'true'
            sparse_image_path = os.path.dirname(sparse_image_info['true'][0])
            if os.path.isdir(sparse_image_path):
               checksparse_build  = mi.get_tool_build('checksparse')
               checksparse_path   = mi.get_build_path(checksparse_build)
               checksparse_path  += mi.get_tool_path('checksparse')
               checksparse_path   = os.path.abspath(checksparse_path)
               checksparse_tool   = checksparse_path + '/checksparse.py'
            if not os.path.exists(sparse_image_dest):
               os.makedirs(sparse_image_dest,mode = 0755)
            os.chdir(sparse_image_dest)
            lg.log("update_common_info:Executing checksparse tool",verbose=0)
            lg.log_exec(['python', checksparse_tool,'-i',raw_program_xml,'-s',sparse_image_path,'-o',rawprogram_unsparse_xml],verbose=0)
            os.chdir(start_dir)
   else:
      lg.log("update_common_info:"+raw_program_xml+" Path doesnot exist. Could not create sparse images.")

#---------------------------------------------------------#
# Generate ipq image                                      #
#---------------------------------------------------------#
if FIT_tool:
   
   #get tool path
   pack_tool = mi.get_build_path(FIT_tool)
   pack_tool += mi.get_tool_path('create_FIT_image')
  #exported_tools = mi.get_build_path(EXP_tools)
  #exported_tools += mi.get_tool_path('exported_tools')
   exported_packages = mi.get_build_path(EXP_packages)
   exported_packages_64 = mi.get_build_path(EXP_packages_64)
   exported_packages += mi.get_tool_path('exported_packages')
   exported_packages_64 += mi.get_tool_path('exported_packages_64')
   meta_tools = mi.get_build_path(META_TOOLS)
   meta_tools += mi.get_tool_path('meta_tools')
   jtag_scripts = mi.get_build_path(JTAG_SCRIPTS)
   jtag_scripts += mi.get_tool_path('jtag-scripts')
   skales_tools = mi.get_build_path(SKALES_tools)
   skales_tools += mi.get_tool_path('skales')

   ipq_dir = './ipq'
   if not os.path.exists(ipq_dir):
      os.makedirs(ipq_dir,mode = 0755)

   bin_dir = './bin'
   if not os.path.exists(bin_dir):
      os.makedirs(bin_dir,mode = 0755)

	  
   bin_debug_dir = './bin_debug'
   if not os.path.exists(bin_debug_dir):
      os.makedirs(bin_debug_dir,mode = 0755)

	  
   # copy all files has attribute flash_file to ipq folder
   for flash_file in mi.get_files(attr='flash_file'):
      shutil.copy(flash_file, ipq_dir)

   for flash_file in mi.get_files(attr='flash_file_V2'):
      shutil.copy(flash_file, ipq_dir) 

   tools_dir = './ipq/tools'
   packages_dir = './ipq/packages'
   packages_dir_64 = './ipq_x64/packages'
   distutils.dir_util.copy_tree( meta_tools , './ipq' )
   distutils.dir_util.copy_tree( exported_packages ,packages_dir)
   distutils.dir_util.copy_tree( skales_tools ,ipq_dir)
   # remove AK t32 contents
   distutils.dir_util.copy_tree( jtag_scripts , '../t32' )
   start_dir = os.getcwd()
   shutil.copy( pack_tool,'./ipq/scripts' )  
   
   #ipq_debug_dir = './ipq_debug'
   #if not os.path.exists(ipq_debug_dir):
   #   os.makedirs(ipq_debug_dir,mode = 0755)
   #   distutils.dir_util.copy_tree(ipq_dir, ipq_debug_dir)
   

   for flash_file in mi.get_files(attr='flash_apss_file'):
      shutil.copy(flash_file, ipq_dir)


   if ((profile=="P") or (profile=="E") or (profile=="Test.gen.int.5")) :
	   ipq_debug_dir_x64 = './ipq_debug_x64'
	   if not os.path.exists(ipq_debug_dir_x64):
		os.makedirs(ipq_debug_dir_x64,mode = 0755)
		distutils.dir_util.copy_tree(ipq_dir, ipq_debug_dir_x64)
   
   
	   ipq_dir_x64 = './ipq_x64'
	   if not os.path.exists(ipq_dir_x64):
		os.makedirs(ipq_dir_x64,mode = 0755)
		distutils.dir_util.copy_tree(ipq_dir, ipq_dir_x64)
		distutils.dir_util.copy_tree(exported_packages_64,packages_dir_64)
  
	   for flash_file in mi.get_files(attr='flash_apss_debug_64_file'):
		shutil.copy(flash_file, ipq_debug_dir_x64)	  
   

	   for flash_file in mi.get_files(attr='flash_apss_64_file'):
		shutil.copy(flash_file, ipq_dir_x64)
	   for flash_file in mi.get_files(attr='flash_file_V2_64'):
		shutil.copy(flash_file, ipq_dir_x64) 
	   lg.log("update_common_info: Executing prepareSingleImage.py and pack command for P/E 64 bit")
	   ret_prep_64image = lg.log_exec(['python', ipq_dir_x64+'/prepareSingleImage.py',"--arch","ipq807x_64","--in", ipq_dir_x64,"--gencdt", "--genbootconf", "--genpart","--genmbn"],verbose=0)
	   ret_pack_64image = lg.log_exec(['python', ipq_dir_x64+"/scripts/pack_hk.py",  '--arch', "ipq807x_64",'--fltype', "nand,norplusnand,emmc,norplusemmc,nor" , '--srcPath', ipq_dir_x64, '--inImage', ipq_dir_x64 ,'--outImage', bin_dir ],verbose=0)
	   ret_pack_apps_64image = lg.log_exec(['python', ipq_dir_x64+"/scripts/pack_hk.py",  '--arch', "ipq807x_64",'--fltype', "nand,norplusnand,emmc,norplusemmc,nor" , '--srcPath', ipq_dir_x64, '--inImage', ipq_dir_x64 ,'--outImage', bin_dir,'--image_type', "hlos" ],verbose=0)
	   
	   lg.log("update_common_info: Executing prepareSingleImage.py and pack command for 32 bit")
	   
	   ret_prep_32image = lg.log_exec(['python',ipq_dir+'/prepareSingleImage.py',"--arch","ipq807x","--in", ipq_dir,"--gencdt", "--genbootconf", "--genpart","--genmbn"],verbose=0)
	   ret_pack_32image = lg.log_exec(['python', pack_tool,  '--arch', "ipq807x",'--fltype', "nand,norplusnand,emmc,norplusemmc" , '--srcPath', ipq_dir, '--inImage', ipq_dir ,'--outImage', bin_dir ],verbose=0)
	   ret_pack_apps_32image = lg.log_exec(['python', pack_tool,  '--arch', "ipq807x",'--fltype', "nand,norplusnand,emmc,norplusemmc" , '--srcPath', ipq_dir, '--inImage', ipq_dir ,'--outImage', bin_dir,'--image_type' , "hlos" ],verbose=0)
		 
	   #lg.log("update_common_info: Executing prepareSingleImage.py and pack command for premium debug 32 bit")
	   
	   #ret_prep_debug_32image = lg.log_exec(['python',ipq_debug_dir+'/prepareSingleImage.py',"--arch","ipq807x","--in", ipq_debug_dir,"--gencdt", "--genbootconf", "--genpart","--genmbn"],verbose=0)
	   #ret_pack_debug_32image = lg.log_exec(['python', pack_tool,  '--arch', "ipq807x",'--fltype', "nand,norplusnand,emmc,norplusemmc,nor" , '--srcPath', ipq_debug_dir, '--inImage', ipq_debug_dir ,'--outImage', bin_debug_dir ],verbose=0)
	  
	   lg.log("update_common_info: Executing prepareSingleImage.py and pack command for premium debug 64 bit")
	   
	   ret_prep_debug_64image = lg.log_exec(['python', ipq_debug_dir_x64+'/prepareSingleImage.py',"--arch","ipq807x_64","--in", ipq_debug_dir_x64,"--gencdt", "--genbootconf", "--genpart","--genmbn"],verbose=0)
	   ret_pack_debug_64image = lg.log_exec(['python', ipq_debug_dir_x64+"/scripts/pack_hk.py",  '--arch', "ipq807x_64",'--fltype', "nand,norplusnand,emmc,norplusemmc,nor" , '--srcPath', ipq_debug_dir_x64, '--inImage', ipq_debug_dir_x64 ,'--outImage', bin_debug_dir ],verbose=0)
	   
	   ###################---------LKBOOT IMAGE GENERATION----------#################

	   
	   lg.log("update_common_info: Executing prepareSingleImage.py and pack command for lkboot 32 bit")
	   
	   ret_prep_32image = lg.log_exec(['python',ipq_dir+'/prepareSingleImage.py',"--arch","ipq807x", "--fltype", "emmc" ,"--in", ipq_dir,"--gencdt", "--genbootconf", "--genpart","--genmbn","--lk"],verbose=0)
	   ret_pack_32image = lg.log_exec(['python', pack_tool,  '--arch', "ipq807x",'--fltype', "emmc" , '--srcPath', ipq_dir, '--inImage', ipq_dir ,'--outImage', bin_dir ,"--lk" ],verbose=0)
	   
	   
	   lg.log("update_common_info: Executing prepareSingleImage.py and pack command for lkboot 64 bit")
	   ret_prep_64image = lg.log_exec(['python',ipq_dir_x64+'/prepareSingleImage.py',"--arch","ipq807x_64", "--fltype", "emmc" ,"--in", ipq_dir_x64,"--gencdt", "--genbootconf", "--genpart","--genmbn","--lk"],verbose=0)
	   ret_pack_64image = lg.log_exec(['python', ipq_dir_x64+"/scripts/pack_hk.py",  '--arch', "ipq807x_64",'--fltype', "emmc" , '--srcPath', ipq_dir_x64, '--inImage', ipq_dir_x64 ,'--outImage', bin_dir, "--lk"],verbose=0)
   
	   #   if ret_prep_64image != 0 or ret_pack_64image !=0 or ret_prep_32image != 0 or ret_pack_32image != 0:
	   #    sys.exit('Required images are missing from common/build/bin directory.Please refer the log for more information')


  	   #for filename in glob.glob('./'):
    	   # new_name = re.sub(pattern, r'\1_\2\3', filename)
     	   #os.rename(filename, new_name)
   	   #lg.log_exec(['python',pack_tool,'-t','nor','-o', 'nor-ipq807x-single.img', '-c', './ipq807x/config.xml', "./" ,"../bin" ,"32"],verbose=0)
   	   #lg.log_exec(['python',pack_tool,'-t','nand','-o', 'nand-ipq807x-single.img', '-c', './ipq807x/config.xml', "./","../bin" ,"32"],verbose=0)
   	   #lg.log_exec(['python',pack_tool,'-t','norplusnand','-o', 'norplusnand_ipq807x-single.img', '-c', './ipq807x/config.xml', "./","../bin" ,"32"],verbose=0)
   	   #lg.log_exec(['python',pack_tool,'-t','emmc','-o', 'emmc_ipq807x-single.img', '-c', './ipq807x/config.xml', "./","../bin" ,"32"],verbose=0)
   	   #lg.log_exec(['python',pack_tool,'-t','norplusemmc','-o', 'norplusemmc_ipq807x-single.img', '-c', './ipq807x/config.xml', "./","../bin" ,"32"],verbose=0)
   	   #os.chdir('../')
	   
   
	   #os.chdir(ipq_dir)
   	   #lg.log("update_common_info: Executing pack command")
   	   #lg.log_exec(['python',pack_tool,'-t','nor','-o', 'nor-ipq807x_64-single.img', '-c', './ipq807x/config.xml', "./" ,"../bin" ,"64"],verbose=0)
   	   #lg.log_exec(['python',pack_tool,'-t','nor','-o', 'nand-ipq807x_64-single.img', '-c', './ipq807x/config.xml', "./","../bin" ,"64"],verbose=0)
   	   #lg.log_exec(['python',pack_tool,'-t','norplusnand','-o', 'norplusnand_ipq807x_64-single.img', '-c', './ipq807x/config.xml', "./","../bin" ,"64"],verbose=0)
   	   #lg.log_exec(['python',pack_tool,'-t','emmc','-o', 'emmc_ipq807x_64-single.img', '-c', './ipq807x/config.xml', "./","../bin" ,"64"],verbose=0)
   	   #lg.log_exec(['python',pack_tool,'-t','norplusemmc','-o', 'norplusemmc_ipq807x_64-single.img', '-c', './ipq807x/config.xml', "./","../bin" ,"64"],verbose=0)

   	   #os.chdir('../')
	   #lg.log_exec(['python',pack_tool,'-t','nor','-B', '-F','boardconfig','-o', bin_dir +'/'+'nor-ipq806x-single.img',ipq_dir],verbose=0)
   	   #lg.log_exec(['python',pack_tool,'-t','norplusnand','-B', '-F','boardconfig','-o', bin_dir +'/'+'nornand-ipq806x-single.img',ipq_dir],verbose=0)
   	   #lg.log_exec(['python',pack_tool,'-t','nand','-B', '-F','appsboardconfig', '-o',bin_dir +'/'+'ipq806x-nand-apps.img',ipq_dir],verbose=0)
   	   #lg.log_exec(['python',pack_tool,'-t','nor','-B', '-F','appsboardconfig', '-o',bin_dir +'/'+'ipq806x-nor-apps.img',ipq_dir],verbose=0)
   	   #lg.log_exec(['python',pack_tool,'-t','norplusnand','-B', '-F','appsboardconfig', '-o', bin_dir +'/'+'ipq806x-nornand-apps.img',ipq_dir],verbose=0)
	   
	   lg.log("update_common_info.py:============ UPDATE COMMON INFO COMPLETE====================",verbose=0)

   elif (profile=="LM512"):
	   ipq_debug_dir_x64 = './ipq_debug_x64'
	   if not os.path.exists(ipq_debug_dir_x64):
		os.makedirs(ipq_debug_dir_x64,mode = 0755)
		distutils.dir_util.copy_tree(ipq_dir, ipq_debug_dir_x64)
   
   
	   ipq_dir_x64 = './ipq_x64'
	   if not os.path.exists(ipq_dir_x64):
		os.makedirs(ipq_dir_x64,mode = 0755)
		distutils.dir_util.copy_tree(ipq_dir, ipq_dir_x64)
		distutils.dir_util.copy_tree(exported_packages_64,packages_dir_64)
  
	   for flash_file in mi.get_files(attr='flash_apss_debug_64_file'):
		shutil.copy(flash_file, ipq_debug_dir_x64)	 
		
	   for flash_file in mi.get_files(attr='flash_apss_64_file'):
		shutil.copy(flash_file, ipq_dir_x64)
	   for flash_file in mi.get_files(attr='flash_file_V2_64'):
		shutil.copy(flash_file, ipq_dir_x64)
	   lg.log("update_common_info: Executing prepareSingleImage.py and pack command for LM512 64 bit")
	   ret_prep_64image = lg.log_exec(['python', ipq_dir_x64+'/prepareSingleImage.py',"--arch","ipq807x_64","--in", ipq_dir_x64,"--gencdt", "--genbootconf", "--genpart","--genmbn","--memory","512"],verbose=0)
	   ret_pack_64image = lg.log_exec(['python', ipq_dir_x64+"/scripts/pack_hk.py",  '--arch', "ipq807x_64",'--fltype', "nand,norplusnand,emmc,norplusemmc,nor" , '--srcPath', ipq_dir_x64, '--inImage', ipq_dir_x64 ,'--outImage', bin_dir ,"--memory","512"],verbose=0)
	   ret_pack_apps_64image = lg.log_exec(['python', ipq_dir_x64+"/scripts/pack_hk.py",  '--arch', "ipq807x_64",'--fltype', "nand,norplusnand,emmc,norplusemmc,nor" , '--srcPath', ipq_dir_x64, '--inImage', ipq_dir_x64 ,'--outImage', bin_dir,'--image_type', "hlos" ,"--memory","512"],verbose=0)
	   
	   lg.log("update_common_info: Executing prepareSingleImage.py and pack command for 32 bit")
	   
	   ret_prep_32image = lg.log_exec(['python',ipq_dir+'/prepareSingleImage.py',"--arch","ipq807x","--in", ipq_dir,"--gencdt", "--genbootconf", "--genpart","--genmbn","--memory","512"],verbose=0)
	   ret_pack_32image = lg.log_exec(['python', pack_tool,  '--arch', "ipq807x",'--fltype', "nand,norplusnand,emmc,norplusemmc" , '--srcPath', ipq_dir, '--inImage', ipq_dir ,'--outImage', bin_dir ,"--memory","512"],verbose=0)
	   ret_pack_apps_32image = lg.log_exec(['python', pack_tool,  '--arch', "ipq807x",'--fltype', "nand,norplusnand,emmc,norplusemmc" , '--srcPath', ipq_dir, '--inImage', ipq_dir ,'--outImage', bin_dir,'--image_type' , "hlos" ,"--memory","512"],verbose=0)

	   
	   lg.log("update_common_info: Executing prepareSingleImage.py and pack command for lkboot 32 bit")
	   
	   ret_prep_32image = lg.log_exec(['python',ipq_dir+'/prepareSingleImage.py',"--arch","ipq807x", "--fltype", "emmc" ,"--in", ipq_dir,"--gencdt", "--genbootconf", "--genpart","--genmbn","--lk","--memory","512"],verbose=0)
	   ret_pack_32image = lg.log_exec(['python', pack_tool,  '--arch', "ipq807x",'--fltype', "emmc" , '--srcPath', ipq_dir, '--inImage', ipq_dir ,'--outImage', bin_dir ,"--lk" ,"--memory","512"],verbose=0)
	   
	   
	   lg.log("update_common_info: Executing prepareSingleImage.py and pack command for lkboot 64 bit")
	   ret_prep_64image = lg.log_exec(['python',ipq_dir_x64+'/prepareSingleImage.py',"--arch","ipq807x_64", "--fltype", "emmc" ,"--in", ipq_dir_x64,"--gencdt", "--genbootconf", "--genpart","--genmbn","--lk","--memory","512"],verbose=0)
	   ret_pack_64image = lg.log_exec(['python', ipq_dir_x64+"/scripts/pack_hk.py",  '--arch', "ipq807x_64",'--fltype', "emmc" , '--srcPath', ipq_dir_x64, '--inImage', ipq_dir_x64 ,'--outImage', bin_dir, "--lk","--memory","512"],verbose=0)
   
	   #   if ret_prep_64image != 0 or ret_pack_64image !=0 or ret_prep_32image != 0 or ret_pack_32image != 0:
	   #    sys.exit('Required images are missing from common/build/bin directory.Please refer the log for more information')

  	   #for filename in glob.glob('./'):
    	   # new_name = re.sub(pattern, r'\1_\2\3', filename)
     	   #os.rename(filename, new_name)
	   
	   os.chdir(ipq_dir)
	   
	   lg.log("update_common_info.py:============ UPDATE COMMON INFO COMPLETE====================",verbose=0)

   elif (profile=="LM256"):
	   lg.log("update_common_info: Executing prepareSingleImage.py and pack command for 32 bit")
	   
	   ret_prep_32image = lg.log_exec(['python',ipq_dir+'/prepareSingleImage.py',"--arch","ipq807x","--in", ipq_dir,"--gencdt", "--genbootconf", "--genpart","--genmbn","--memory","256"],verbose=0)
	   ret_pack_32image = lg.log_exec(['python', pack_tool,  '--arch', "ipq807x",'--fltype', "nand,norplusnand,emmc,norplusemmc" , '--srcPath', ipq_dir, '--inImage', ipq_dir ,'--outImage', bin_dir ,"--memory","256"],verbose=0)
	   ret_pack_apps_32image = lg.log_exec(['python', pack_tool,  '--arch', "ipq807x",'--fltype', "nand,norplusnand,emmc,norplusemmc" , '--srcPath', ipq_dir, '--inImage', ipq_dir ,'--outImage', bin_dir,'--image_type' , "hlos" ,"--memory","256"],verbose=0)

	   
	   lg.log("update_common_info: Executing prepareSingleImage.py and pack command for lkboot 32 bit")
	   
	   ret_prep_32image = lg.log_exec(['python',ipq_dir+'/prepareSingleImage.py',"--arch","ipq807x", "--fltype", "emmc" ,"--in", ipq_dir,"--gencdt", "--genbootconf", "--genpart","--genmbn","--lk","--memory","256"],verbose=0)
	   ret_pack_32image = lg.log_exec(['python', pack_tool,  '--arch', "ipq807x",'--fltype', "emmc" , '--srcPath', ipq_dir, '--inImage', ipq_dir ,'--outImage', bin_dir ,"--lk" ,"--memory","256"],verbose=0)
	   
	   
	   #   if ret_prep_64image != 0 or ret_pack_64image !=0 or ret_prep_32image != 0 or ret_pack_32image != 0:
	   #    sys.exit('Required images are missing from common/build/bin directory.Please refer the log for more information')
   	   #g.log_exec(['python','

   	   #for filename in glob.glob('./'):
    	   # new_name = re.sub(pattern, r'\1_\2\3', filename)
     	   #os.rename(filename, new_name)
	   
	   os.chdir(ipq_dir)
	   
	   lg.log("update_common_info.py:============ UPDATE COMMON INFO COMPLETE====================",verbose=0)

   else:
	   lg.log("update_common_info.py:============ YOU ARE AT WRONG PLACE ====================",verbose=0)
