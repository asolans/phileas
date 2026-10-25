Intenció

Volem crear una aplicació de processament de text, exclusivament de consola i per a Línux.

L'aplicació es dirà Phileas. Tindrà el repositori git en aquesta carpeta.

En primer lloc decidirem el llenguatge de programació (C,C++,Lua,?).  Analitzant els requeriments de l'aplicació:

- Destinat a escriure text, no codi. Per tant ha de contenir:
    - Ha de treballar en format markdown. Només marcatge, sense previsualització. Potser aplicant colors a Headers. Mínim en qualsevol cas.
    - Pot necessitar funcionar en plaques Raspberry pi de molt pocs recursos.
    - Wordwrap i definició de nombre de columnes per línia.
    - Marcatge mínim (en format md)
    - Eines d'edició, cerca-substitució, diccionaris, etc
    - Edita només un document, però hem de poder navegar pel disc per obrir nous fitxers
    - Etc Anirem definint les necssitats. Aquesta llista és per a decidir l'arquitectura.

- Obre un document de seguiment de l'aplicació
