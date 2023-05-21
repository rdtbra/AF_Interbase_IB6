C  The contents of this file are subject to the Interbase Public
C  License Version 1.0 (the "License"); you may not use this file
C  except in compliance with the License. You may obtain a copy
C  of the License at http://www.Inprise.com/IPL.html
C
C  Software distributed under the License is distributed on an
C  "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
C  or implied. See the License for the specific language governing
C  rights and limitations under the License.
C
C  The Original Code was created by Inprise Corporation
C  and its predecessors. Portions created by Inprise Corporation are
C  Copyright (C) Inprise Corporation.
C
C  All Rights Reserved.
C  Contributor(s): ______________________________________.

      BLOCK DATA                   ! Interbase global initialization

      INTEGER*4  GDS__TRANS        ! { default transaction handle }
      COMMON /GDS__TRANS/GDS__TRANS
      DATA GDS__TRANS /0/          ! { init trans handle }
C
C    UNCOMMENT THE FOLLOWING LINES IN ORDER TO USE PYXIS
C    WITH WINDOWS WITH GLOBAL SCOPE
C
C     INTEGER*4  GDS__WINDOW        %s{ window handle }
C     INTEGER*2  GDS__HEIGHT        %s{ window height }
C     INTEGER*2  GDS__WIDTH         %s{ window width }
C   
C     COMMON /GDS__WINDOW/GDS__WINDOW
C     COMMON /GDS__HEIGHT/GDS__HEIGHT
C     COMMON /GDS__WIDTH/GDS__WIDTH
C   
C
C     DATA GDS__WINDOW /0/           %s{ init window handle }
C     DATA GDS__HEIGHT /24/           %s{ init window height }
C     DATA GDS__WIDTH /80/           %s{ init window width }
      END
