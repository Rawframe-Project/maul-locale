# The catalog benchmark's set: 2,000 keys in en and fr, a quarter each
# plain text, a placeholder, a plural selection and two numbers. Run as
# cmake -DDIR=<directory> -P catalog_set.cmake.
set(en "@locale en\n---\n")
set(fr "@locale fr\n---\n")
foreach(i RANGE 1999)
    math(EXPR kind "${i} % 4")
    string(LENGTH "${i}" digits)
    math(EXPR pad "4 - ${digits}")
    string(REPEAT "0" ${pad} zeros)
    set(key "screen.item.${zeros}${i}")
    if(kind EQUAL 0)
        string(APPEND en "${key} = Item ${i}\n")
        string(APPEND fr "${key} = Élément ${i}\n")
    elseif(kind EQUAL 1)
        string(APPEND en "${key} = Hello {$name}, item ${i}\n")
        string(APPEND fr "${key} = Bonjour {$name}, élément ${i}\n")
    elseif(kind EQUAL 2)
        string(APPEND en "${key} = .input {$n :integer} .match $n one {{One file in ${i}}} "
                         "* {{{$n} files in ${i}}}\n")
        string(APPEND fr "${key} = .input {$n :integer} .match $n one {{Un fichier dans ${i}}} "
                         "* {{{$n} fichiers dans ${i}}}\n")
    else()
        string(APPEND en "${key} = Saved {$n :integer} of {$total :integer} in ${i}\n")
        string(APPEND fr "${key} = {$n :integer} sur {$total :integer} enregistrés dans ${i}\n")
    endif()
endforeach()
file(WRITE ${DIR}/en.mf2 "${en}")
file(WRITE ${DIR}/fr.mf2 "${fr}")
