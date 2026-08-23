
function build () {
  meson compile -C build
}

function dev () {                                                     
  cd build
  ./elma
  #valgrind --tool=callgrind node ts/test.js                          
}                                                                     
function debug () {                                                     
  cd build
  gdb elma
}                                                                     
                                                                      

function build_font_atlas () {                                                             
  (                                                                                          
      cd build                                                                               
      fat=font_atlas.h                                                                       
      echo '#include <map>' > $fat                                                           
      echo '#include <vector>' >> $fat                                                       
      for f in font-atlas*.png; do                                                           
        xxd -i $f >> $fat                                                                    
      done                                                                                   
      sed -i 's/unsigned char \(.*\)\[\]/std::vector<uint8_t> \1/g' $fat                     
      echo "std::map<std::string, std::vector<uint8_t>*> font_atlases = {{" >> $fat          
      sed -n 's/.* \(font_atlas_\(.*\)_png\) .*/{"\2", \&\1}, /p' $fat >> $fat               
      echo "}};" >> $fat                                                                     
    )                                                                                        
}                                                                                            

                                                                      
                                                                      
if [ -z "$1" ]; then                                                  
  echo "Commands:"                                                    
  echo                                                                
  cat $0 | sed -rne 's/^function ([^_][^ \(]+).*/  \1/p'              
  echo                                                                
else                                                                  
                                                                      
  cmd=$1           # Get the function name from argv                  
  shift            # Remove function name                             
  eval $cmd $@     # Call function and parse arguments                
fi                                                                    
                                                                      

